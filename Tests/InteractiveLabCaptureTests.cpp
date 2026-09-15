#include "FLabCaptureQueue.h"
#include "FLabCaptureExport.h"
#include "Core/FPlatformFileSystem.h"
#include "VulkanRHI/FVulkanDevice.h"
#include <iostream>
#include <limits>

namespace
{
using namespace Stoner;
using namespace Stoner::Demo;
using namespace Stoner::RHI;
FLabCaptureRequest Request(Core::uint64 Id=1)
{
    FLabCaptureRequest R;
    R.RequestId=Id;
    R.Target.SettingsGeneration=R.Target.DisplayGeneration=R.Target.OutputGeneration=1;
    R.Target.Width=R.Target.Height=16;
    R.Target.Format=ERHIFormat::R8G8B8A8_UNorm;
    R.Target.OutputProfile="Sdr.sRGB.v1"; R.Target.Stage="FinalOutput";
    return R;
}
FLabCaptureFrame Frame(const FLabCaptureRequest& R,Core::uint64 Token=1)
{ return {Token,R.Target,Renderer::EFrameExecutionPurpose::InteractivePreview,true}; }
}
int RunInteractiveLabCaptureTests()
{
    int Failed=0;
    auto Check=[&](bool OK,const char* Name) { std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n'; if (!OK) ++Failed; };
    {
        FLabCaptureExportTask Task;
        std::atomic<bool> Release{false},Entered{false};
        Core::FString Result;
        const bool Started=Task.Start([&] {
            Entered.store(true);
            while (!Release.load()) std::this_thread::yield();
            return Core::FString("published");
        });
        Check(Started && !Task.Start([] { return Core::FString("extra"); }),
            "capture export task admits exactly one worker");
        while (Started && !Entered.load()) std::this_thread::yield();
        bool Pending=true;
        for (unsigned I=0;I<1000;++I) Pending=Pending && !Task.Poll(Result);
        Check(Pending && Task.IsActive(),"blocked publication leaves event polling available and retains worker ownership");
        Release.store(true);
        while (Task.IsActive() && !Task.Poll(Result)) std::this_thread::yield();
        Check(Result=="published" && !Task.IsActive(),"completed publication is collected exactly once");
        Check(Task.Start([]() -> Core::FString { throw 1; }),"capture worker can be reused after retirement");
        while (Task.IsActive() && !Task.Poll(Result)) std::this_thread::yield();
        Check(Result=="Capture export failed unexpectedly.","capture worker exception becomes a controlled failure");
    }
    {
        FLabCaptureCompletion C{Request(),7,ELabCaptureStatus::Success};
        Core::TArray<Core::uint8> RGBA;
        for (unsigned I=0;I<256;++I) RGBA.insert(RGBA.end(),{17,34,51,255});
        FLabCaptureEncoded Encoded;
        Check(EncodeLabCapture(C,RGBA,"fixture-software",Encoded) && Encoded.bPNG &&
            Encoded.Payload.size()>8 && Encoded.Payload[0]==137 && !Encoded.Report.empty(),
            "SDR capture encodes a bounded PNG and identity report");
        auto BGRA=RGBA;
        for (size_t I=0;I<BGRA.size();I+=4) std::swap(BGRA[I],BGRA[I+2]);
        C.Request.Target.Format=ERHIFormat::B8G8R8A8_UNorm;
        FLabCaptureEncoded Swizzled;
        Check(EncodeLabCapture(C,BGRA,"fixture-software",Swizzled) && Swizzled.Payload==Encoded.Payload,
            "BGRA capture preserves exact channels when encoding PNG");
        (void)Core::FPlatformFileSystem::CreateDirectory("Build/Validation/030");
        Check(Core::FPlatformFileSystem::WriteFile("Build/Validation/030/capture-codec.png",Encoded.Payload) &&
            Core::FPlatformFileSystem::WriteFile("Build/Validation/030/capture-codec.json",Encoded.Report),
            "capture codec writes ignored artifacts for independent PNG and JSON verification");
        C.Request.Target.Format=ERHIFormat::R16G16B16A16_Float;
        C.Request.Target.Purpose=ELabCapturePurpose::HDRNumeric;
        C.Request.Target.Stage="ManualExposure";
        const Core::TArray<Core::uint8> Numeric(16*16*8,0);
        Check(EncodeLabCapture(C,Numeric,"fixture-software",Swizzled) && !Swizzled.bPNG && Swizzled.Payload==Numeric,
            "numeric capture preserves raw bytes without generating an HDR PNG");
        C.Request.Target.Purpose=ELabCapturePurpose::SDRPreview;
        Check(!EncodeLabCapture(C,Numeric,"fixture-software",Swizzled),
            "SDR encoder rejects float16 diagnostic data instead of converting appearance");
        C.Request.Target=Request().Target;
        C.Status=ELabCaptureStatus::Cancelled;
        Check(!EncodeLabCapture(C,RGBA,"fixture-software",Swizzled),
            "cancelled capture cannot publish encoded success");
        C.Status=ELabCaptureStatus::Success; RGBA.pop_back();
        Check(!EncodeLabCapture(C,RGBA,"fixture-software",Swizzled),
            "capture codec rejects a truncated exact-size readback");
    }
    auto Device=Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
    Backend::Vulkan::FVulkanInstanceDesc Desc;
    Desc.RuntimeMode=Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
    Check(Device->Initialize(Desc)==ERHIResult::Success,"capture deterministic device initializes");
    auto Command=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    Check(Command && Command->Begin()==ERHIResult::Success,"capture command begins");
    if (!Command) return 1;
    {
        FLabCaptureQueue Q;
        bool Rejected=true;
        for (int Case=0;Case<6;++Case)
        {
            auto R=Request();
            if (Case==0) R.RequestId=0;
            if (Case==1) R.Target.DisplayGeneration=0;
            if (Case==2) R.Target.Width=4097;
            if (Case==3) R.Target.Width=R.Target.Height=4096;
            if (Case==4) R.Target.Format=ERHIFormat::Unknown;
            if (Case==5) R.Target.Purpose=static_cast<ELabCapturePurpose>(99);
            Rejected &= Q.Request(R,100)==ELabCaptureStatus::InvalidRequest;
        }
        Check(Rejected && Q.GetStatistics().Requests==0 && Q.GetStatistics().StagingBytes==0,
            "invalid identities, axes, pixel product, format and purpose reject before resource ownership");
    }
    {
        FLabCaptureQueue Q;
        FLabCapturePrepared P;
        Check(Q.PrepareNext(Frame(Request()),Device,Command,100,P)==ELabCaptureStatus::Pending &&
            Q.GetStatistics().Requests==0 && Q.GetStatistics().StagingBytes==0 && !P.Staging,
            "ordinary frames allocate no capture staging without an explicit request");
        Check(Q.Request(Request(),100)==ELabCaptureStatus::Pending && Q.Request(Request(2),100)==ELabCaptureStatus::Pending &&
            Q.Request(Request(3),100)==ELabCaptureStatus::Busy && Q.GetStatistics().PeakRequests==2,
            "pending requests consume both queue entries and the third returns Busy");
        Check(Q.Request(Request(),100)==ELabCaptureStatus::InvalidRequest,"capture request IDs cannot alias a live or past request");
        Check(Q.Cancel(1) && Q.GetStatistics().Requests==2,"cancellation retains its bounded result until consumed");
        FLabCaptureCompletion C;
        Check(Q.ProcessOne(1,100,{},C) && C.Status==ELabCaptureStatus::Cancelled && C.Request.RequestId==1,
            "pending cancellation completes without a readback");
        Q.CancelAll();
        Check(!Q.ProcessOne(1,100,{},C) && Q.ProcessOne(2,100,{},C) && Q.GetStatistics().Requests==0,
            "repeated service frame cannot consume a second completion");
    }
    for (int Change=0;Change<7;++Change)
    {
        FLabCaptureQueue Q;
        const auto R=Request(); (void)Q.Request(R,100);
        auto F=Frame(R);
        switch (Change)
        {
        case 0: ++F.Identity.Width; break;
        case 1: ++F.Identity.SettingsGeneration; break;
        case 2: ++F.Identity.DisplayGeneration; break;
        case 3: ++F.Identity.OutputGeneration; break;
        case 4: F.Identity.bIncludeUI=true; break;
        case 5: F.Identity.OutputProfile="Sdr.BT709.Gamma22.v1"; break;
        default: F.Identity.Stage="SceneColor"; break;
        }
        FLabCapturePrepared P;
        FLabCaptureCompletion C;
        Check(Q.PrepareNext(F,Device,Command,101,P)==ELabCaptureStatus::GenerationMismatch &&
            !P.Staging && Q.ProcessOne(1,101,{},C) && C.Status==ELabCaptureStatus::GenerationMismatch,
            "extent/settings/display/output/UI/profile/stage mismatch rejects without allocation or retargeting");
    }
    {
        FLabCaptureQueue Q;
        auto R=Request(); (void)Q.Request(R,100);
        auto F=Frame(R); F.ExecutionPurpose=Renderer::EFrameExecutionPurpose::FormalValidation;
        FLabCapturePrepared P;
        Check(Q.PrepareNext(F,Device,Command,101,P)==ELabCaptureStatus::InvalidRequest &&
            Q.GetStatistics().StagingBytes==0 && Q.GetStatistics().Requests==1,
            "formal execution cannot consume or mutate an interactive capture request");
        F.ExecutionPurpose=Renderer::EFrameExecutionPurpose::InteractivePreview; F.bStable=false;
        Check(Q.PrepareNext(F,Device,Command,102,P)==ELabCaptureStatus::Pending && !P.Staging,
            "transition or zero-extent intervals cannot allocate capture staging");
        FLabCaptureCompletion C;
        Check(Q.ProcessOne(1,5100,{},C) && C.Status==ELabCaptureStatus::TimedOut &&
            Q.GetStatistics().TimedOut==1,"unrecorded capture has a five-second deadline");
    }
    {
        FLabCaptureQueue Q;
        auto R=Request(); R.Target.bIncludeUI=true;
        Check(Q.Request(R,100)==ELabCaptureStatus::Pending,"explicit UI-inclusive capture is accepted");
        FLabCapturePrepared P;
        Check(Q.PrepareNext(Frame(R,4),Device,Command,101,P)==ELabCaptureStatus::Success && P.Staging &&
            P.Staging->GetSizeInBytes()==16*16*4,"matching frame reserves exact-size readback staging");
        auto Fence=Device->CreateFence(false).Object;
        Check(Command->End()==ERHIResult::Success && Q.Submit(1,4,Fence),"capture retains its exact submitted frame fence");
        P={};
        FLabCaptureCompletion C;
        int Reads=0;
        auto Read=[&](const auto& Result,const auto&,const auto&) { ++Reads; return Result.FrameToken==4 && Result.Request.Target.bIncludeUI; };
        Check(!Q.ProcessOne(1,200,Read,C) && Reads==0,"unsignaled capture fence never maps or waits for readback");
        Check(Fence->Signal()==ERHIResult::Success,"capture fixture independently completes render");
        Q.Poll(201);
        Check(Fence->Reset()==ERHIResult::Success && Command->Reset()==ERHIResult::Success,
            "render retirement resets fence and command after completion observation");
        Q.Poll(202);
        Check(Command->Begin()==ERHIResult::Success,"capture fixture reuses retired command");
        Check(Q.ProcessOne(2,202,Read,C) && C.Status==ELabCaptureStatus::Success && Reads==1 &&
            Q.GetStatistics().StagingBytes==0,"observed capture survives fence and command reuse and reads exact UI-inclusive identity once");
    }
    {
        FLabCaptureQueue Q; (void)Q.Request(Request(),100);
        FLabCapturePrepared P;
        (void)Q.PrepareNext(Frame(Request()),Device,Command,100,P);
        auto Fence=Device->CreateFence(false).Object;
        (void)Command->End(); (void)Q.Submit(1,1,Fence); P={};
        (void)Fence->Signal(); Q.Poll(101); (void)Command->Reset();
        Core::TSharedPtr<IRHIBuffer> Retained;
        int Reads=0;
        auto Reader=[&](const auto&,const auto& Buffer,const auto&) { ++Reads; Retained=Buffer; return true; };
        FLabCaptureCompletion C;
        Check(!Q.ProcessOne(1,101,Reader,C) && Reads==1 && Q.GetStatistics().Requests==1 &&
            Q.GetStatistics().StagingBytes==1024,
            "consumer staging alias retains request capacity and bytes after readback");
        Check(!Q.ProcessOne(2,102,Reader,C) && Reads==1,
            "retained consumer alias cannot trigger another readback");
        Retained.reset();
        Check(Q.ProcessOne(3,103,Reader,C) && Reads==1 && C.Status==ELabCaptureStatus::Success &&
            Q.GetStatistics().Requests==0 && Q.GetStatistics().StagingBytes==0,
            "releasing consumer alias publishes the saved result without repeating readback");
        (void)Command->Begin();
    }
    for (bool Timeout : {false,true})
    {
        FLabCaptureQueue Q; (void)Q.Request(Request(),100);
        FLabCapturePrepared P;
        Check(Q.PrepareNext(Frame(Request()),Device,Command,100,P)==ELabCaptureStatus::Success,"retained capture prepares");
        auto Fence=Device->CreateFence(false).Object;
        Check(Command->End()==ERHIResult::Success && Q.Submit(1,1,Fence),"retained capture submits");
        P={};
        if (!Timeout) Check(Q.Cancel(1),"active capture accepts cancellation");
        FLabCaptureCompletion C;
        Check(!Q.ProcessOne(1,5100,{},C) && Q.GetStatistics().Requests==1 && Q.GetStatistics().StagingBytes>0,
            "cancelled or timed-out active capture retains staging until real completion");
        (void)Fence->Signal(); Q.Poll(5101); (void)Command->Reset();
        Check(Q.ProcessOne(2,5101,{},C) && C.Status==(Timeout ? ELabCaptureStatus::TimedOut : ELabCaptureStatus::Cancelled) &&
            Q.GetStatistics().StagingBytes==0,"retirement releases failed capture without invoking a reader");
        (void)Command->Begin();
    }
    {
        FLabCaptureQueue Q;
        auto R=Request(); R.Target.Width=R.Target.Height=2048; R.Target.Format=ERHIFormat::R32G32B32A32_Float;
        R.Target.Purpose=ELabCapturePurpose::HDRNumeric; R.Target.OutputProfile="Hdr.scRGB.v1";
        (void)Q.Request(R,100); R.RequestId=2; (void)Q.Request(R,100);
        FLabCapturePrepared A,B;
        Check(Q.PrepareNext(Frame(R,10),Device,Command,100,A)==ELabCaptureStatus::Success &&
            Q.PrepareNext(Frame(R,11),Device,Command,100,B)==ELabCaptureStatus::Success &&
            Q.GetStatistics().StagingBytes==FLabCaptureQueue::MaximumStagingBytes,
            "two 64 MiB allocations exactly fill the 128 MiB aggregate cap");
        Q.CancelAll(); A={}; B={}; (void)Command->End(); (void)Command->Reset(); Q.Poll(101);
        FLabCaptureCompletion C;
        Check(Q.ProcessOne(1,101,{},C) && !Q.ProcessOne(1,101,{},C) && Q.ProcessOne(2,101,{},C) &&
            Q.GetStatistics().StagingBytes==0,"discarded commands release cancelled staging one result per frame");
        (void)Command->Begin();
    }
    {
        FLabCaptureQueue Q;
        auto R=Request(); R.Target.Width=2048; R.Target.Height=2049; R.Target.Format=ERHIFormat::R32G32B32A32_Float;
        R.Target.Purpose=ELabCapturePurpose::HDRNumeric; R.Target.OutputProfile="Hdr.scRGB.v1";
        (void)Q.Request(R,100); R.RequestId=2; (void)Q.Request(R,100);
        FLabCapturePrepared A,B;
        Check(Q.PrepareNext(Frame(R,10),Device,Command,100,A)==ELabCaptureStatus::Success &&
            Q.PrepareNext(Frame(R,11),Device,Command,100,B)==ELabCaptureStatus::Busy && !B.Staging &&
            Q.GetStatistics().StagingBytes<FLabCaptureQueue::MaximumStagingBytes,
            "aggregate staging overflow refuses the second allocation without evicting the first");
        Q.CancelAll(); A={}; (void)Command->End(); (void)Command->Reset(); Q.Poll(101);
        FLabCaptureCompletion C; (void)Q.ProcessOne(1,101,{},C); (void)Q.ProcessOne(2,101,{},C);
        (void)Command->Begin();
    }
    {
        FLabCaptureQueue Q; (void)Q.Request(Request(),100);
        Device->ConfigureAllocationBudget(1);
        FLabCapturePrepared P;
        FLabCaptureCompletion C;
        Check(Q.PrepareNext(Frame(Request()),Device,Command,100,P)==ELabCaptureStatus::AllocationFailed &&
            !P.Staging && Q.GetStatistics().StagingBytes==0 &&
            Q.ProcessOne(1,100,{},C) && C.Status==ELabCaptureStatus::AllocationFailed,
            "allocation failure publishes a bounded failure without consuming staging");
        Device->ResetResourceConfiguration();
        Check(Q.Request(Request(2),100)==ELabCaptureStatus::Pending &&
            Q.PrepareNext(Frame(Request(2)),Device,Command,100,P)==ELabCaptureStatus::Success,
            "new explicit request can recover after allocation failure");
        auto Fence=Device->CreateFence(false).Object;
        (void)Command->End(); (void)Q.Submit(2,1,Fence); (void)Fence->Signal(); Q.Poll(101);
        (void)Command->Reset();
        Check(!Q.ProcessOne(2,101,{},C) && Q.GetStatistics().StagingBytes>0,
            "staging aliases retain budget until the recording caller releases them");
        P={};
        Check(Q.ProcessOne(3,101,[](const auto&,const auto&,const auto&) -> bool { throw 1; },C) &&
            C.Status==ELabCaptureStatus::ReadbackFailed && Q.GetStatistics().StagingBytes==0,
            "readback consumer failure is explicit and releases only completed staging");
        (void)Command->Begin();
    }
    {
        FLabCaptureQueue Q;
        (void)Q.Request(Request(),100);
        Q.Poll(5099); Q.Poll(0);
        FLabCaptureCompletion C;
        Check(!Q.ProcessOne(1,5099,{},C) && Q.ProcessOne(2,5100,{},C) && C.Status==ELabCaptureStatus::TimedOut,
            "a decreasing clock cannot extend the capture deadline");
    }
    {
        FLabCaptureQueue Q; (void)Q.Request(Request(),100);
        FLabCapturePrepared P;
        (void)Q.PrepareNext(Frame(Request()),Device,Command,100,P);
        auto Fence=Device->CreateFence(false).Object;
        (void)Command->End(); (void)Q.Submit(1,1,Fence); P={};
        Check(!Q.ReleaseAfterDeviceShutdown(*Device) && Q.GetStatistics().StagingBytes>0,
            "active-device teardown cannot discard a pending capture fence");
        auto Other=Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
        (void)Other->Initialize(Desc); (void)Other->Shutdown();
        Check(!Q.ReleaseAfterDeviceShutdown(*Other),"foreign device teardown cannot release capture ownership");
        Check(Device->Shutdown()==ERHIResult::Success && Q.ReleaseAfterDeviceShutdown(*Device),
            "actual device teardown permits abandoned capture host cleanup");
        FLabCaptureCompletion C;
        Check(Q.ProcessOne(1,101,{},C) && C.Status==ELabCaptureStatus::DeviceLost &&
            Q.GetStatistics().StagingBytes==0 && Q.Request(Request(2),101)==ELabCaptureStatus::InvalidRequest,
            "abandoned capture remains failed and a torn-down queue cannot restart");
    }
    return Failed ? 1 : 0;
}
