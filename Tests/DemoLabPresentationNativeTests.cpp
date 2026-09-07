#include "FDemoBackendFactory.h"
#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "RHI/RHIMinimal.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
using namespace Stoner;
using namespace Stoner::RHI;

void Check(int& Failed, bool Passed, const char* Name)
{
    Failed += Passed ? 0 : 1;
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

template<class Operation>
ERHIResult PollBounded(Application::FWindow& Window, Operation&& Poll)
{
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(3);
    for (;;)
    {
        (void)Window.PollEvents();
        const auto Result = Poll();
        if (Result != ERHIResult::NotReady && Result != ERHIResult::Timeout)
            return Result;
        if (std::chrono::steady_clock::now() >= Deadline)
            return ERHIResult::Timeout;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void RunCase(int& Failed, Demo::EDemoGraphicsBackend Backend, bool ForceAcquireHistory)
{
    std::cout << "[INFO] Demo lab backend=" << static_cast<int>(Backend)
              << " forceAcquireHistory=" << ForceAcquireHistory << '\n';
    Application::FWindow Window;
    Application::FWindowDesc WindowDesc;
    WindowDesc.Title = "Stoner Demo Lab Presentation Bridge";
    WindowDesc.ClientWidth = 160;
    WindowDesc.ClientHeight = 96;
    WindowDesc.bVisible = true;
    auto Created = Demo::FDemoBackendFactory().Create(Backend);
    const bool Ready = Created.Succeeded() &&
        Window.CreateRealWindow(WindowDesc) == Application::EApplicationResult::Success &&
        Created.Runtime->InitializeLab(Window.GetPlatformWindow(), 2, false,
            ForceAcquireHistory) == ERHIResult::Success;
    Check(Failed, Ready, "Demo lab factory initializes the requested native backend");
    if (!Ready)
    {
        if (Created.Runtime) (void)Created.Runtime->Shutdown();
        (void)Window.Destroy();
        return;
    }
    auto& Runtime = *Created.Runtime;
    auto Device = Runtime.GetDevice();
    Demo::FDemoLabPresentationStatus Status;
    const auto Query = Runtime.QueryLabPresentation(Status);
    FRHIPresentationFormatColorSpacePair Pair;
    for (const auto& Candidate : Status.Capabilities.SupportedPairs)
    {
        if ((Candidate.ColorSpace == ERHIPresentationColorSpace::SrgbNonlinear ||
             Candidate.ColorSpace == ERHIPresentationColorSpace::SdrPassThrough) &&
            (Candidate.Format == ERHIFormat::B8G8R8A8_UNorm ||
             Candidate.Format == ERHIFormat::R8G8B8A8_UNorm))
        {
            Pair = Candidate;
            break;
        }
    }
    FRHISwapchainDesc Request;
    Request.Width = Window.GetDrawableWidth();
    Request.Height = Window.GetDrawableHeight();
    Request.FramesInFlight = 2;
    Request.PreferredFormat = Pair.Format;
    Request.PreferredColorSpace = Pair.ColorSpace;
    Request.SurfaceCapabilityGeneration = Status.Capabilities.CapabilityGeneration;
    const auto Prepared = Pair.IsValid()
        ? Runtime.PrepareLabPresentation(Request, Status) : ERHIResult::Unsupported;
    Check(Failed, Query == ERHIResult::Success && Prepared == ERHIResult::Success &&
            Status.bPrepared && Status.ResolvedState.IsValid() &&
            (!ForceAcquireHistory ||
                Status.RetirementMode == ERHIPresentationRetirementMode::AcquireHistory),
        "Demo lab capabilities prepare a native SDR target and honor forced acquire history");
    const auto Queue = Device->CreateCommandQueue(ERHIQueueType::Graphics);
    bool FramesPassed = Prepared == ERHIResult::Success && Queue.Succeeded();
    if (FramesPassed)
    {
        auto PausedRequest = Request;
        PausedRequest.Width = 0;
        const auto Paused = Runtime.ReconfigureLabPresentation(PausedRequest, Status);
        FRHIBorrowedAcquiredTarget PausedTarget;
        const auto PausedAcquire = Runtime.AcquireLabTarget(499999, 0, PausedTarget);
        const auto PausedQuery = Runtime.QueryLabPresentation(Status);
        Request.SurfaceCapabilityGeneration = Status.Capabilities.CapabilityGeneration;
        const auto Resumed = PollBounded(Window, [&] {
            return Runtime.ReconfigureLabPresentation(Request, Status);
        });
        FramesPassed = Paused == ERHIResult::NotReady &&
            PausedAcquire == ERHIResult::NotReady && !PausedTarget.IsValid() &&
            Resumed == ERHIResult::Success;
        if (!FramesPassed)
            std::cerr << "lab pause=" << static_cast<int>(Paused)
                      << " acquire=" << static_cast<int>(PausedAcquire)
                      << " query=" << static_cast<int>(PausedQuery)
                      << " resume=" << static_cast<int>(Resumed)
                      << " capabilityGeneration=" << Request.SurfaceCapabilityGeneration
                      << " reason=" << Status.FailureReason.ToStdString() << '\n';
        Check(Failed, FramesPassed,
            "Demo lab zero-axis reconfiguration pauses acquisition and resumes on a valid drawable");
    }
    std::vector<FRHIPresentationLease> Leases;
    Core::uint32 CompletedFrames = 0;
    for (Core::uint32 Index = 0; FramesPassed && Index < 10; ++Index)
    {
        const auto Token = static_cast<Core::uint64>(500000 + Index);
        const auto Slot = Index % 2;
        FRHIBorrowedAcquiredTarget Target;
        const auto Acquire = PollBounded(Window, [&] {
            return Runtime.AcquireLabTarget(Token, Slot, Target);
        });
        FramesPassed = Acquire == ERHIResult::Success && Target.IsValid() &&
            Target.Frame.FrameToken == Token && Target.FrameSlotIndex == Slot;
        if (!FramesPassed)
        {
            std::cerr << "lab acquire frame=" << Index << " result="
                      << static_cast<int>(Acquire) << '\n';
            break;
        }
        if (Index == 0)
        {
            FRHIBorrowedAcquiredTarget Again;
            const auto Repeated = Runtime.AcquireLabTarget(Token, Slot, Again);
            FRHIBorrowedAcquiredTarget Conflict;
            const auto Rejected = Runtime.AcquireLabTarget(Token + 100, Slot, Conflict);
            Check(Failed, Repeated == ERHIResult::Success && Target.Matches(Again) &&
                    Rejected != ERHIResult::Success && !Conflict.IsValid(),
                "Demo acquire retries preserve the exact target and reject a conflicting slot identity");
        }
        FRHIRenderPassDesc PassDesc;
        PassDesc.Attachments = {{ERHIAttachmentRole::Color, Target.Frame.Format,
            ERHISampleCount::One, ERHIAttachmentLoadOp::Clear,
            ERHIAttachmentStoreOp::Store}};
        const auto Pass = Device->CreateRenderPass(PassDesc);
        FRHIFramebufferDesc FramebufferDesc;
        FramebufferDesc.RenderPass = Pass.Object;
        FramebufferDesc.Attachments = {{Target.Texture}};
        FramebufferDesc.Width = Target.Frame.Width;
        FramebufferDesc.Height = Target.Frame.Height;
        const auto Framebuffer = Device->CreateFramebuffer(FramebufferDesc);
        const auto Command = Device->CreateCommandBuffer(ERHIQueueType::Graphics);
        const auto Fence = Device->CreateFence(false);
        FRHIRenderPassClearValues Clear;
        Clear.Colors = {{0.05f, 0.15f, 0.25f, 1.0f}};
        FRHIResourceBarrierDesc ToPresent;
        ToPresent.Texture = Target.Texture;
        ToPresent.RequiredTextureUsage = ERHITextureUsage::Present;
        ToPresent.Before = ERHIResourceLayout::ColorAttachment;
        ToPresent.After = ERHIResourceLayout::Present;
        FramesPassed = Pass.Succeeded() && Framebuffer.Succeeded() &&
            Command.Succeeded() && Fence.Succeeded() &&
            Command.Object->Begin() == ERHIResult::Success &&
            Command.Object->BeginRenderPass(Pass.Object, Framebuffer.Object, Clear) == ERHIResult::Success &&
            Command.Object->EndRenderPass() == ERHIResult::Success &&
            Command.Object->RecordLayoutTransition(ToPresent) == ERHIResult::Success &&
            Command.Object->End() == ERHIResult::Success;
        Core::TArray<Core::TSharedPtr<IRHISemaphore>> Waits;
        if (Target.AcquireSemaphore) Waits.push_back(Target.AcquireSemaphore);
        FramesPassed = FramesPassed &&
            Queue.Object->SubmitDeferred(Command.Object, Waits, {}, Fence.Object) == ERHIResult::Success &&
            PollBounded(Window, [&] { return Fence.Object->Wait(0); }) == ERHIResult::Success;
        if (!FramesPassed) break;
        FRHIRenderLease RenderLease;
        RenderLease.Frame = Target.Frame;
        RenderLease.FrameSlotIndex = Slot;
        RenderLease.CompletionFence = Fence.Object;
        if (Index == 0)
        {
            FRHIPresentationLease ForgedPresentation;
            ForgedPresentation.Frame = Target.Frame;
            ForgedPresentation.PresentationCompletionFence = Fence.Object;
            bool ForgedCompletion = true;
            Check(Failed, Runtime.PollLabPresentation(ForgedPresentation,
                    ForgedCompletion) == ERHIResult::InvalidState && !ForgedCompletion,
                "Demo presentation polling rejects a render fence that was never admitted as a presentation lease");
            auto Stale = RenderLease;
            ++Stale.Frame.FrameToken;
            FRHIPresentationLease RejectedLease;
            Check(Failed, Runtime.PresentLabTarget(Target, Stale, RejectedLease) ==
                    ERHIResult::InvalidState && !RejectedLease.IsValid(),
                "Demo presentation rejects mismatched render proof before native admission");
        }
        FRHIPresentationLease Lease;
        const auto Presented = PollBounded(Window, [&] {
            return Runtime.PresentLabTarget(Target, RenderLease, Lease);
        });
        FramesPassed = Presented == ERHIResult::Success && Lease.Matches(Target);
        if (!FramesPassed)
        {
            std::cerr << "lab present frame=" << Index << " result="
                      << static_cast<int>(Presented) << '\n';
            break;
        }
        Leases.push_back(Lease);
        ++CompletedFrames;
        for (auto It = Leases.begin(); It != Leases.end();)
        {
            bool Complete = false;
            const auto Poll = Runtime.PollLabPresentation(*It, Complete);
            if (Poll != ERHIResult::Success && Poll != ERHIResult::NotReady)
            {
                FramesPassed = false;
                break;
            }
            if (Complete) It = Leases.erase(It);
            else ++It;
        }
    }
    Check(Failed, FramesPassed && CompletedFrames == 10,
        "Demo lab renders ten borrowed native clear frames with deferred submission and typed presentation");
    if (FramesPassed)
    {
        FRHIBorrowedAcquiredTarget CancelTarget;
        const auto Acquired = PollBounded(Window, [&] {
            return Runtime.AcquireLabTarget(600000, 0, CancelTarget);
        });
        bool Acknowledged = false;
        const auto Cancelled = Acquired == ERHIResult::Success
            ? PollBounded(Window, [&] {
                return Runtime.CancelLabTarget(600000, 0, nullptr, Acknowledged);
            }) : Acquired;
        Check(Failed, Acquired == ERHIResult::Success &&
                Cancelled == ERHIResult::Success && Acknowledged,
            "Demo cancellation acknowledges an unsubmitted borrowed frame without claiming presentation completion");
    }
    if (FramesPassed && Backend == Demo::EDemoGraphicsBackend::Metal)
    {
        FRHIBorrowedAcquiredTarget PendingTarget;
        const auto Pending = Runtime.AcquireLabTarget(600001, 0, PendingTarget);
        bool Acknowledged = true;
        const auto Wrong = Runtime.CancelLabTarget(600002, 0, nullptr, Acknowledged);
        Check(Failed, Pending == ERHIResult::NotReady && !PendingTarget.IsValid() &&
            Wrong == ERHIResult::InvalidState && !Acknowledged,
            "pending Metal acquire remains private and rejects a foreign cancellation token");
        const auto Cancelled = PollBounded(Window, [&] {
            return Runtime.CancelLabTarget(600001, 0, nullptr, Acknowledged);
        });
        const auto After = Runtime.QueryLabPresentation(Status);
        Check(Failed, Cancelled == ERHIResult::Success && Acknowledged &&
            After == ERHIResult::Success && Status.PendingAcquireCount == 0,
            "pending Metal cancellation waits for its native job without acquiring a public target");
    }
    // Terminal native teardown is permitted here. The test runner imposes an
    // external process deadline; this is not the Application T030 watchdog.
    const auto Shutdown = PollBounded(Window, [&] { return Runtime.Shutdown(); });
    Check(Failed, Shutdown == ERHIResult::Success,
        "Demo lab terminal shutdown completes without reusing formal presentation APIs");
    Leases.clear();
    Device.reset();
    (void)Window.Destroy();
}
} // namespace

int RunDemoLabPresentationNativeTests()
{
    if (std::getenv("STONER_REQUIRE_DEMO_LAB_NATIVE") == nullptr)
    {
        std::cout << "[SKIP] Demo lab native bridge requires STONER_REQUIRE_DEMO_LAB_NATIVE\n";
        return 0;
    }
    int Failed = 0;
    RunCase(Failed, Demo::EDemoGraphicsBackend::Vulkan, false);
    RunCase(Failed, Demo::EDemoGraphicsBackend::Vulkan, true);
#if defined(__APPLE__)
    RunCase(Failed, Demo::EDemoGraphicsBackend::Metal, false);
#endif
    return Failed == 0 ? 0 : 1;
}
