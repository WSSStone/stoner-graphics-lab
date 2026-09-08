#include "FUITextureRegistry.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanCommandBuffer.h"
#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
FUITextureRequest Create(Core::uint32 Slot = 1, Core::uint32 Size = 4)
{
    FUITextureRequest R;
    R.RequestId = Slot; R.LogicalSlot = Slot; R.Width = R.Height = Size;
    R.Format = ERHIFormat::R8G8B8A8_UNorm;
    R.ColorDomain = EUITextureColorDomain::AlphaCoverage;
    R.PixelBytes.assign(static_cast<std::size_t>(Size) * Size * 4, 255);
    return R;
}
}
int RunUITextureRegistryTests()
{
    int Failed = 0;
    auto Check = [&](bool OK, const char* Name)
    {
        std::cout << (OK ? "[PASS] " : "[FAIL] ") << Name << '\n';
        if (!OK) ++Failed;
    };
    auto Device = Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
    Backend::Vulkan::FVulkanInstanceDesc DeviceDesc;
    DeviceDesc.RuntimeMode = Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
    const bool Initialized = Device->Initialize(DeviceDesc) == ERHIResult::Success;
    Check(Initialized, "UI registry deterministic device initializes");
    if (!Initialized) return Failed;
    {
        FUITextureRegistry Registry(Device);
        auto Request = Create();
        Check(Registry.Prepare(Request).Result == ERHIResult::NotReady,
            "paused registry does not allocate or upload");
        Registry.BeginEligibleFrame(1, true);
        const auto First = Registry.Prepare(Request);
        Check(First.Succeeded() && First.State == EUITextureState::Prepared,
            "UI creation prepares resource without claiming GPU completion");
        auto OldLease = Registry.Acquire(First.TextureId);
        Check(OldLease.IsValid(), "prepared generation acquires a snapshot lease");
        Request.Operation = EUITextureOperation::Update;
        Request.LogicalSlot = 0; Request.TextureId = First.TextureId;
        Request.ExpectedGeneration = First.TextureId.Generation;
        Request.PixelBytes[3] = 128;
        const auto Second = Registry.Prepare(Request);
        Check(Second.Succeeded() && Second.TextureId.IsNewerThan(First.TextureId) &&
            Registry.GetStatistics().Generations == 2,
            "copy-on-write update retains leased old generation");
        Check(Registry.Prepare(Request).Result == ERHIResult::InvalidState,
            "stale expected generation cannot replace current texture");
        Registry.Poll();
        Check(Registry.GetStatistics().Generations == 2,
            "retirement cannot discard a prepared snapshot lease");
        OldLease = {}; Registry.Poll();
        Check(Registry.GetStatistics().Generations == 1 && !Registry.Acquire(First.TextureId).IsValid(),
            "superseded unsubmitted generation retires after final lease");
        auto Lease = Registry.Acquire(Second.TextureId);
        auto Command = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        auto Fence = Device->CreateFence(false).Object;
        auto PresentFence = Device->CreateFence(false).Object;
        Check(Command->Begin() == ERHIResult::Success, "UI upload command begins");
        FUITextureSubmission Submission;
        Check(Registry.RecordSubmission({&Lease, 1}, Command, Submission) == ERHIResult::Success,
            "UI upload is explicitly ordered before sampling in one command stream");
        const auto NativeCommand = std::dynamic_pointer_cast<Backend::Vulkan::FVulkanCommandBuffer>(Command);
        Core::TArray<Core::uint8> Shadow;
        bool CopiedShadow = false;
        for (const auto& Recorded : NativeCommand->GetRecordedCommands())
            if (Recorded.Type == ERHISymbolicCommandType::BufferToTextureCopy)
                CopiedShadow = Device->ReadbackBufferForTesting(Recorded.BufferA, 0, 1024, Shadow) == ERHIResult::Success;
        Check(CopiedShadow && Shadow[0] == 255 && Shadow[3] == 128 && Shadow[256 + 3] == 255,
            "copy-on-write staging contains the complete padded shadow and unchanged rows");
        Check(Submission.Commit(Fence) == ERHIResult::Success,
            "accepted render submission attaches its completion fence");
        FUITextureRequest Destroy;
        Destroy.RequestId = 90; Destroy.Operation = EUITextureOperation::Destroy;
        Destroy.TextureId = Second.TextureId; Destroy.ExpectedGeneration = Second.TextureId.Generation;
        Check(Registry.Prepare(Destroy).Result == ERHIResult::NotReady,
            "destroy acknowledgement waits for snapshot and render owners");
        Lease = {}; Submission = {}; Command.reset(); Registry.Poll();
        Check(Registry.GetStatistics().Generations == 1,
            "unsignaled render fence retains old texture without snapshot lease");
        Check(Fence->Signal() == ERHIResult::Success, "fixture independently signals render completion");
        Registry.Poll();
        Check(Registry.GetStatistics().Generations == 0 && !PresentFence->IsSignaled() &&
            Registry.Prepare(Destroy).State == EUITextureState::Destroyed,
            "render-complete present-pending texture retires and acknowledges destroy");
        Registry.BeginEligibleFrame(2, true);
        const auto Recreated = Registry.Prepare(Create());
        Check(Recreated.TextureId.IsNewerThan(Second.TextureId),
            "reused logical slot never reuses a stale generation identity");
    }
    {
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1, true);
        for (Core::uint32 Slot = 1; Slot <= 64; ++Slot) (void)Registry.Prepare(Create(Slot));
        Check(Registry.GetStatistics().Generations == 64 &&
            Registry.Prepare(Create(65)).Result == ERHIResult::NotReady,
            "eligible-frame request budget stops the sixty-fifth preparation");
        Registry.BeginEligibleFrame(1, true);
        Check(Registry.Prepare(Create(65)).Result == ERHIResult::NotReady,
            "repeating frame identity cannot reset request budget");
        Registry.BeginEligibleFrame(2, false);
        Check(Registry.Prepare(Create(65)).Result == ERHIResult::NotReady,
            "minimized frame performs no new texture preparation");
        Registry.BeginEligibleFrame(3, true);
        Check(Registry.Prepare(Create(65)).Succeeded(), "new eligible frame resumes bounded preparation");
    }
    {
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1, true);
        auto Request = Create();
        const auto First = Registry.Prepare(Request);
        const auto Before = Device->GetAllocationSnapshot();
        Device->ConfigureAllocationBudget(Before.AllocatedBytes + 64);
        Request.Operation = EUITextureOperation::Update; Request.LogicalSlot = 0;
        Request.TextureId = First.TextureId; Request.ExpectedGeneration = First.TextureId.Generation;
        const auto Rejected = Registry.Prepare(Request);
        Check(!Rejected.Succeeded() && Registry.Acquire(First.TextureId).IsValid() &&
            Registry.GetStatistics().Generations == 1 &&
            Device->GetAllocationSnapshot().AllocatedBytes == Before.AllocatedBytes,
            "staging allocation failure rolls back texture and preserves current ID");
        Device->ConfigureAllocationBudget(0);
        const auto Second = Registry.Prepare(Request);
        Check(Second.Succeeded() && Second.TextureId.Generation == First.TextureId.Generation + 1,
            "failed preparation consumes neither generation nor current identity");
        auto Lease = Registry.Acquire(Second.TextureId);
        auto Command = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        (void)Command->Begin();
        {
            FUITextureSubmission Cancelled;
            Check(Registry.RecordSubmission({&Lease, 1}, Command, Cancelled) == ERHIResult::Success,
                "unsubmitted frame reserves its upload generation");
            auto OtherCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
            (void)OtherCommand->Begin();
            FUITextureSubmission Other;
            Check(Registry.RecordSubmission({&Lease, 1}, OtherCommand, Other) == ERHIResult::NotReady &&
                OtherCommand->GetRecordedCommandCount() == 0,
                "same prepared upload cannot be concurrently recorded into two frame slots");
            Command.reset();
        }
        Command = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        (void)Command->Begin();
        FUITextureSubmission Retry;
        Registry.BeginEligibleFrame(1, false);
        Check(Registry.RecordSubmission({&Lease, 1}, Command, Retry) == ERHIResult::NotReady &&
            Command->GetRecordedCommandCount() == 0,
            "pause with unchanged frame identity prevents prepared uploads");
        Registry.BeginEligibleFrame(2, true);
        Check(Registry.RecordSubmission({&Lease, 1}, Command, Retry) == ERHIResult::Success,
            "discarded unsubmitted command releases reservation for retry");
    }
    {
        FUITextureRegistry Registry(Device);
        Core::TArray<FUITextureLease> OldLeases;
        Core::uint64 Frame = 0;
        bool AllPrepared = true;
        for (Core::uint32 Slot = 1; Slot <= 256; ++Slot)
        {
            if ((Slot - 1) % 64 == 0) Registry.BeginEligibleFrame(++Frame, true);
            const auto Prepared = Registry.Prepare(Create(Slot));
            AllPrepared = AllPrepared && Prepared.Succeeded();
            OldLeases.push_back(Registry.Acquire(Prepared.TextureId));
        }
        for (Core::uint32 Slot = 1; Slot <= 256; ++Slot)
        {
            if ((Slot - 1) % 64 == 0) Registry.BeginEligibleFrame(++Frame, true);
            auto Request = Create(Slot);
            Request.Operation = EUITextureOperation::Update; Request.LogicalSlot = 0;
            Request.TextureId = OldLeases[Slot - 1].GetId();
            Request.ExpectedGeneration = Request.TextureId.Generation;
            AllPrepared = AllPrepared && Registry.Prepare(Request).Succeeded();
        }
        Check(AllPrepared && Registry.GetStatistics().Generations == 512,
            "active and leased retired generations share the 512-record budget");
        Registry.BeginEligibleFrame(++Frame, true);
        auto Request = Create(); Request.Operation = EUITextureOperation::Update;
        Request.LogicalSlot = 0; Request.TextureId = {1, 2}; Request.ExpectedGeneration = 2;
        Check(Registry.Prepare(Request).Result == ERHIResult::NotReady &&
            Registry.GetStatistics().Generations == 512,
            "record saturation defers replacement without evicting leased generations");
        OldLeases.clear(); Registry.Poll();
        Check(Registry.GetStatistics().Generations == 256 && Registry.Prepare(Request).Succeeded(),
            "released retired records restore preparation capacity");
        Check(!Registry.Prepare(Create(257)).Succeeded(), "logical slot 257 rejects");
    }
    {
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1, true);
        const auto First = Registry.Prepare(Create(1, 2048));
        Check(First.Succeeded() && Registry.Prepare(Create(2)).Result == ERHIResult::NotReady,
            "a sixteen-MiB texture exhausts this eligible frame's upload payload");
        Registry.BeginEligibleFrame(2, true);
        const auto Second = Registry.Prepare(Create(2, 2048));
        Registry.BeginEligibleFrame(3, true);
        const auto Stats = Registry.GetStatistics();
        Check(Second.Succeeded() && Stats.GPUBytes + Stats.StagingBytes == 64ull * 1024 * 1024 &&
            Stats.CPUShadowBytes == 32ull * 1024 * 1024 &&
            Registry.Prepare(Create(3)).Result == ERHIResult::NotReady,
            "texture and padded upload storage obey the aggregate allocation ceiling");
        FUITextureRequest Destroy;
        Destroy.RequestId = 99; Destroy.Operation = EUITextureOperation::Destroy;
        Destroy.TextureId = First.TextureId; Destroy.ExpectedGeneration = First.TextureId.Generation;
        Check(Registry.Prepare(Destroy).State == EUITextureState::Destroyed &&
            Registry.Prepare(Create(3)).Succeeded(),
            "unleased unsubmitted destruction refunds byte budgets without waiting");
    }
    {
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1, true);
        const auto Prepared = Registry.Prepare(Create());
        auto Lease = Registry.Acquire(Prepared.TextureId);
        auto UploadCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        (void)UploadCommand->Begin();
        FUITextureSubmission Upload;
        (void)Registry.RecordSubmission({&Lease, 1}, UploadCommand, Upload);
        auto UploadFence = Device->CreateFence(false).Object;
        (void)Upload.Commit(UploadFence);
        auto FirstCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        (void)FirstCommand->Begin();
        FUITextureSubmission First;
        Check(Registry.RecordSubmission({&Lease, 1}, FirstCommand, First) == ERHIResult::NotReady &&
            FirstCommand->GetRecordedCommandCount() == 0,
            "a different command stream cannot sample an uncompleted upload without dependency");
        (void)UploadFence->Signal(); Registry.Poll(); Upload = {}; UploadCommand.reset();
        Check(Registry.GetStatistics().StagingBytes == 0 && Registry.GetStatistics().CPUShadowBytes == 1024,
            "render completion releases staging while retaining the active CPU shadow");
        auto SecondCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        (void)SecondCommand->Begin();
        FUITextureSubmission Second;
        const bool FirstRecorded = Registry.RecordSubmission({&Lease, 1}, FirstCommand, First) == ERHIResult::Success;
        const bool SecondRecorded = Registry.RecordSubmission({&Lease, 1}, SecondCommand, Second) == ERHIResult::Success;
        Check(FirstRecorded && SecondRecorded && FirstCommand->GetRecordedCommandCount() == 0 &&
            SecondCommand->GetRecordedCommandCount() == 0,
            "two frame slots can sample a ready generation without repeat uploads");
        auto ThirdCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        (void)ThirdCommand->Begin();
        FUITextureSubmission Third;
        Check(Registry.RecordSubmission({&Lease, 1}, ThirdCommand, Third) == ERHIResult::NotReady,
            "a third concurrent render use cannot exceed two frame slots");
        auto FirstFence = Device->CreateFence(false).Object;
        auto SecondFence = Device->CreateFence(false).Object;
        (void)First.Commit(FirstFence); (void)Second.Commit(SecondFence);
        FUITextureRequest Destroy;
        Destroy.RequestId = 80; Destroy.Operation = EUITextureOperation::Destroy;
        Destroy.TextureId = Prepared.TextureId; Destroy.ExpectedGeneration = Prepared.TextureId.Generation;
        (void)Registry.Prepare(Destroy);
        Lease = {}; First = {}; Second = {};
        FirstCommand.reset(); SecondCommand.reset(); ThirdCommand.reset();
        (void)FirstFence->Signal(); Registry.Poll();
        Check(Registry.GetStatistics().Generations == 1,
            "one completed render use cannot retire another pending render use");
        (void)SecondFence->Signal(); Registry.Poll();
        Check(Registry.GetStatistics().Generations == 0,
            "final render completion retires the unleased destroyed generation");
    }
    {
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1,true);
        FRenderGraph Graph("GPU widget");
        auto Builder=Graph.CreateBuilder();
        const auto Usage=ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled;
        const auto Resource=Builder.CreateTexture("widget",4,4,ERHIFormat::R8G8B8A8_sRGB,
            ERHISampleCount::One,Usage,ERenderGraphColorDomain::EncodedSrgb);
        auto ProducerDesc=FRenderGraphPassDesc::Make("diagnostic",ERenderGraphPassType::Graphics);
        ProducerDesc.Accesses.push_back({Resource,ERenderGraphAccessType::Write,ERenderGraphResourceState::Write});
        const auto Producer=Builder.AddPass(ProducerDesc);
        auto ConsumerDesc=FRenderGraphPassDesc::Make("terminal UI",ERenderGraphPassType::Graphics);
        ConsumerDesc.bPreserveForSideEffects=true;
        ConsumerDesc.Accesses.push_back({Resource,ERenderGraphAccessType::Read,ERenderGraphResourceState::Read});
        const auto Consumer=Builder.AddPass(ConsumerDesc);
        Check(Graph.Compile()==ERenderGraphResult::Success,"GPU widget compiles a producer-before-sample dependency");
        FRHITextureDesc Desc; Desc.Width=Desc.Height=4; Desc.Format=ERHIFormat::R8G8B8A8_sRGB; Desc.Usage=Usage;
        auto Texture=Device->CreateTexture(Desc).Object;
        FUIGpuTextureRegistration Request{1,{},Texture,Resource,Producer};
        FUIGpuTextureContext Context{&Graph,Consumer,1,2,3};
        FUITextureId Id;
        auto Wrong=Context; Wrong.Consumer=Producer;
        Check(Registry.RegisterGpuTexture(Request,Wrong,Id)==ERHIResult::InvalidState && !Id.IsValid(),
            "GPU widget rejects missing or self-referential consumer dependency");
        Check(Registry.RegisterGpuTexture(Request,Context,Id)==ERHIResult::Success && Id.IsValid() &&
            Registry.GetStatistics().GPUBytes==64 && Registry.GetStatistics().CPUShadowBytes==0 &&
            Registry.GetStatistics().StagingBytes==0,"GPU registration retains a sampled sRGB target without CPU shadow or upload");
        FUITextureId Rejected;
        Request.Previous=Id;
        Check(Registry.RegisterGpuTexture(Request,Context,Rejected)==ERHIResult::NotReady && !Rejected.IsValid(),
            "a live GPU target cannot be registered again for rewriting under a new generation");
        Request.Previous={}; Request.LogicalSlot=2;
        auto InvalidDesc=Desc; InvalidDesc.Format=ERHIFormat::R8G8B8A8_UNorm;
        Request.Texture=Device->CreateTexture(InvalidDesc).Object;
        Check(Registry.RegisterGpuTexture(Request,Context,Rejected)==ERHIResult::InvalidState,
            "encoded diagnostic pixels require sampled sRGB rather than encoded UNorm filtering");
        Request.Texture=Texture; Request.LogicalSlot=1;
        Wrong=Context; ++Wrong.FrameId;
        Check(Registry.RegisterGpuTexture(Request,Wrong,Rejected)==ERHIResult::NotReady,
            "GPU registration cannot use a different eligible frame");
        auto Lease=Registry.Acquire(Id);
        Check(Registry.CanRecordSubmission({&Lease,1})==ERHIResult::InvalidState,
            "GPU widget sampling requires its exact graph and frame identities");
        Wrong=Context; ++Wrong.SettingsRevision;
        Check(Registry.CanRecordSubmission({&Lease,1},&Wrong)==ERHIResult::InvalidState &&
            Registry.CanRecordSubmission({&Lease,1},&Context)==ERHIResult::Success,
            "stale settings cannot sample a GPU widget generation");
        Wrong=Context; ++Wrong.DisplayGeneration;
        Check(Registry.CanRecordSubmission({&Lease,1},&Wrong)==ERHIResult::InvalidState,
            "a changed display generation invalidates GPU widget sampling");
        FUITextureRequest CpuDestroy; CpuDestroy.RequestId=900; CpuDestroy.Operation=EUITextureOperation::Destroy;
        CpuDestroy.TextureId=Id; CpuDestroy.ExpectedGeneration=Id.Generation;
        Check(Registry.Prepare(CpuDestroy).Result==ERHIResult::InvalidState && Registry.Acquire(Id).IsValid(),
            "CPU texture acknowledgements cannot destroy Renderer-owned widget generations");
        auto Command=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        (void)Command->Begin(); FUITextureSubmission Submission;
        Check(Registry.RecordSubmission({&Lease,1},Command,Submission,&Context)==ERHIResult::Success &&
            std::dynamic_pointer_cast<Backend::Vulkan::FVulkanCommandBuffer>(Command)->GetRecordedCommands().empty(),
            "GPU registration records no upload, readback or implicit transition");
        auto Fence=Device->CreateFence(false).Object;
        (void)Submission.Commit(Fence);
        Check(Registry.RetireGpuTexture(Id)==ERHIResult::NotReady && !Registry.Acquire(Id).IsValid(),
            "GPU unregistration stops new leases while preserving queued consumers");
        Lease={}; Submission={}; Registry.Poll();
        Check(Registry.GetStatistics().Generations==1,"GPU generation remains retained until its render fence completes");
        (void)Fence->Signal(); Registry.Poll();
        Check(Registry.GetStatistics().Generations==0 && Texture,
            "GPU retirement releases the registry lease without destroying the target owner's texture");
        Request.Texture=Texture; Request.Previous={};
        FUITextureId Next;
        Check(Registry.RegisterGpuTexture(Request,Context,Next)==ERHIResult::Success && Next.IsNewerThan(Id),
            "fully retired GPU storage can be registered with a new monotonic texture identity");
        auto NextLease=Registry.Acquire(Next);
        (void)Builder.AddDependency(Consumer,Producer);
        Check(Graph.Compile()!=ERenderGraphResult::Success &&
            Registry.CanRecordSubmission({&NextLease,1},&Context)==ERHIResult::InvalidState,
            "a graph made cyclic after registration cannot authorize GPU sampling");

    }
    (void)Device->Shutdown();
    return Failed;
}
