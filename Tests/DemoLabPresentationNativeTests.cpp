#include "FDemoBackendFactory.h"
#include "FInteractiveLabRun.h"
#include "Application/FWindow.h"
#include "Application/FInteractiveLabSession.h"
#include "Application/FLabSettingsSnapshot.h"
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
    Check(Failed, Query == ERHIResult::Success && !Status.bPrepared,
        "Demo unprepared surface does not claim a usable output");
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
            Status.RuntimeSnapshot.NativePresentation.ActiveGeneration ==
                Status.ResolvedState.SwapchainImageGeneration &&
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
        const bool PausedUnprepared = !Status.bPrepared;
        Request.SurfaceCapabilityGeneration = Status.Capabilities.CapabilityGeneration;
        const auto Resumed = PollBounded(Window, [&] {
            return Runtime.ReconfigureLabPresentation(Request, Status);
        });
        FramesPassed = Paused == ERHIResult::NotReady &&
            PausedAcquire == ERHIResult::NotReady && !PausedTarget.IsValid() &&
            PausedQuery == ERHIResult::Success && PausedUnprepared &&
            Resumed == ERHIResult::Success && Status.bPrepared &&
            Status.RuntimeSnapshot.NativePresentation.ActiveGeneration ==
                Status.ResolvedState.SwapchainImageGeneration;
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

        FRHIBorrowedAcquiredTarget ClosingTarget;
        const auto ClosingAcquire = PollBounded(Window, [&] {
            return Runtime.AcquireLabTarget(600003, 0, ClosingTarget);
        });
        const auto ClosingCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics);
        const auto ClosingFence = Device->CreateFence(false);
        FRHIRenderPassDesc ClosingPassDesc;
        ClosingPassDesc.Attachments = {{ERHIAttachmentRole::Color, ClosingTarget.Frame.Format,
            ERHISampleCount::One, ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store}};
        const auto ClosingPass = Device->CreateRenderPass(ClosingPassDesc);
        FRHIFramebufferDesc ClosingFramebufferDesc;
        ClosingFramebufferDesc.RenderPass = ClosingPass.Object;
        ClosingFramebufferDesc.Attachments = {{ClosingTarget.Texture}};
        ClosingFramebufferDesc.Width = ClosingTarget.Frame.Width;
        ClosingFramebufferDesc.Height = ClosingTarget.Frame.Height;
        const auto ClosingFramebuffer = Device->CreateFramebuffer(ClosingFramebufferDesc);
        FRHIRenderPassClearValues ClosingClear;
        ClosingClear.Colors = {{0.0f, 0.0f, 0.0f, 1.0f}};
        const bool Rendered = ClosingAcquire == ERHIResult::Success &&
            ClosingCommand.Succeeded() && ClosingFence.Succeeded() &&
            ClosingPass.Succeeded() && ClosingFramebuffer.Succeeded() &&
            ClosingCommand.Object->Begin() == ERHIResult::Success &&
            ClosingCommand.Object->BeginRenderPass(ClosingPass.Object,
                ClosingFramebuffer.Object, ClosingClear) == ERHIResult::Success &&
            ClosingCommand.Object->EndRenderPass() == ERHIResult::Success &&
            ClosingCommand.Object->End() == ERHIResult::Success &&
            Queue.Object->SubmitDeferred(ClosingCommand.Object, {}, {},
                ClosingFence.Object) == ERHIResult::Success &&
            PollBounded(Window, [&] { return ClosingFence.Object->Wait(0); }) == ERHIResult::Success;
        FRHIRenderLease ClosingRender;
        ClosingRender.Frame = ClosingTarget.Frame;
        ClosingRender.FrameSlotIndex = 0;
        ClosingRender.CompletionFence = ClosingFence.Object;
        FRHIPresentationLease ClosingPresentation;
        const bool Closed = Window.RequestClose() == Application::EApplicationResult::Success;
        const auto ClosingPresent = Rendered && Closed
            ? Runtime.PresentLabTarget(ClosingTarget, ClosingRender, ClosingPresentation)
            : ERHIResult::Failed;
        Acknowledged = false;
        const auto ClosingCancel = Rendered ? PollBounded(Window, [&] {
            return Runtime.CancelLabTarget(600003, 0, ClosingFence.Object, Acknowledged);
        }) : ERHIResult::Failed;
        if (ClosingPresent != ERHIResult::NotReady ||
            ClosingCancel != ERHIResult::Success || !Acknowledged)
            std::cerr << "close regression acquire=" << static_cast<int>(ClosingAcquire)
                      << " rendered=" << Rendered << " closed=" << Closed
                      << " present=" << static_cast<int>(ClosingPresent)
                      << " cancel=" << static_cast<int>(ClosingCancel)
                      << " acknowledged=" << Acknowledged << '\n';
        Check(Failed, ClosingPresent == ERHIResult::NotReady &&
            !ClosingPresentation.IsValid() && ClosingCancel == ERHIResult::Success && Acknowledged,
            "Metal close between render and present preserves the target for bounded cancellation");
    }
    (void)Runtime.QueryLabPresentation(Status);
    const auto& LiveOperations = Status.RuntimeSnapshot.NativeOperations;
    Check(Failed, LiveOperations.bAvailable && LiveOperations.ImageReadbackCopyCount == 0 &&
        LiveOperations.ReadbackMapCount == 0 && LiveOperations.ReadbackWaitCount == 0 &&
        LiveOperations.QueueIdleCallCount == 0 && LiveOperations.DeviceIdleCallCount == 0 &&
        LiveOperations.SubmittedRenderCount >= 10 && LiveOperations.SuccessfulRenderCompletionCount >= 10,
        "native clear preview records real submissions with zero readback and ordinary idle calls");
    // Terminal native teardown is permitted here. The test runner imposes an
    // external process deadline; this is not the Application T030 watchdog.
    const auto Shutdown = PollBounded(Window, [&] { return Runtime.Shutdown(); });
    Check(Failed, Shutdown == ERHIResult::Success,
        "Demo lab terminal shutdown completes without reusing formal presentation APIs");
    Demo::FDemoLabPresentationStatus Terminal;
    const auto TerminalQuery = Runtime.QueryLabPresentation(Terminal);
    const auto ExpectedAssurance = Status.RetirementMode == RHI::ERHIPresentationRetirementMode::AcquireHistory
        ? RHI::ERHIShutdownAssurance::IdleAssumed : RHI::ERHIShutdownAssurance::Proven;
    Check(Failed, TerminalQuery == ERHIResult::Success && Terminal.bTerminalDrainComplete &&
        Terminal.ShutdownAssurance == ExpectedAssurance && Terminal.RetainedFacadeOwnerCount == 0,
        "terminal status preserves actual native retirement assurance after facade owners are released");
    const auto& NativeTerminal = Terminal.RuntimeSnapshot.NativePresentation;
    const auto& TerminalOperations = Terminal.RuntimeSnapshot.NativeOperations;
    Check(Failed, NativeTerminal.bAvailable && NativeTerminal.ActiveGeneration == 0 &&
        NativeTerminal.RetiringGeneration == 0 && NativeTerminal.PresentationOwnerCount == 0 &&
        NativeTerminal.AcquisitionRecordCount == 0 && NativeTerminal.PendingAcquireCount == 0 &&
        NativeTerminal.ResidualNativeOwners == 0 && NativeTerminal.AbandonedNativeOwners == 0 &&
        NativeTerminal.PeakEstimatedColorBytes > 0 && TerminalOperations.RetainedSubmissionOwnerCount == 0 &&
        TerminalOperations.ProvenPresentationReleaseCount >= LiveOperations.ProvenPresentationReleaseCount &&
        (Backend == Demo::EDemoGraphicsBackend::Metal
            ? NativeTerminal.TerminalIdleCallCount == 0
            : NativeTerminal.TerminalIdleCallCount == 1 && NativeTerminal.bTerminalIdleCompleted &&
                NativeTerminal.TerminalIdleNativeResult == 0),
        "native terminal accounting retains actual idle result, byte high-water and zero remaining owners");
    Leases.clear();
    Device.reset();
    (void)Window.Destroy();
}
void RunSceneLifecycle(int& Failed)
{
    const auto Env = [](const char* Name) { const char* Value = std::getenv(Name); return Core::FString(Value ? Value : ""); };
    if (Env("STONER_LAB_SCENE_COOK_ROOT").IsEmpty()) return;
    Demo::FDemoConfiguration Config;
    Config.bInteractiveLab = true; Config.bLabUI = false;
    Config.RunMode = Demo::EDemoRunMode::BoundedNative;
    Config.GraphicsBackend = Env("STONER_LAB_SCENE_BACKEND") == "metal"
        ? Demo::EDemoGraphicsBackend::Metal : Demo::EDemoGraphicsBackend::Vulkan;
    Config.bLabForceAcquireHistory = Env("STONER_LAB_SCENE_FORCE_FALLBACK") == "1";
    Config.Workload = Demo::EDemoWorkload::ProductionContent;
    Config.RenderPath = Demo::EDemoRenderPath::DeferredFull;
    Config.ClientWidth = 320; Config.ClientHeight = 180;
    // OS minimize/restore is asynchronous. Close after observing the whole
    // scenario instead of racing an eight-frame budget against its callbacks.
    Config.FrameBudget = 4096; Config.MemorySampleInterval = 120;
    Config.MaxMemoryGrowthBytes = 16ULL * 1024ULL * 1024ULL; Config.MaxMemoryGrowthPercent = 10;
    Config.CookedPublicationRoot = Env("STONER_LAB_SCENE_COOK_ROOT");
    Config.StrictGeneration = Env("STONER_LAB_SCENE_GENERATION");
    Config.ProductionRoot = Env("STONER_LAB_SCENE_ROOT");
    Config.WorkloadRevision = Env("STONER_LAB_SCENE_WORKLOAD");
    Config.TargetProfilePath = Env("STONER_LAB_SCENE_PROFILE");
    Config.LeaseCoordinationRoot = Env("STONER_LAB_SCENE_LEASE_ROOT");
    bool SettingsEdited = false;
    bool ActionsSucceeded = true, ObservedMinimized = false, ObservedRestored = false;
    int Stage = 0;
    Core::uint32 Cycle = 0, CycleStartPresented = 0;
    int FocusStep = 0;
    Core::uint32 PresentedAtRestore = 0;
    auto ScenarioStarted = std::chrono::steady_clock::now();
    bool ScenarioEntered = false;
    auto MinimizeStarted = std::chrono::steady_clock::now();
    const auto Result = Demo::RunInteractiveLab(Config, Demo::FDemoBackendFactory(),
        [&](Application::FWindow& Window, Core::uint32 Presented) {
            if (!ScenarioEntered) { ScenarioStarted = std::chrono::steady_clock::now(); ScenarioEntered = true; }
            if (std::chrono::steady_clock::now() - ScenarioStarted > std::chrono::seconds(20))
            {
                ActionsSucceeded = false;
                (void)Window.RequestClose();
                return;
            }
            if (Stage == 0 && Presented >= CycleStartPresented + 2)
            {
                ActionsSucceeded &= Window.SetClientSize(Cycle % 2 == 0 ? 352 : 320, Cycle % 2 == 0 ? 198 : 180) == Application::EApplicationResult::Success;
                Stage = 1;
            }
            else if (Stage == 1 && Presented >= CycleStartPresented + 4)
            {
                if (FocusStep < 4)
                {
                    Window.QueueEvent(FocusStep % 2 == 0 ? Application::FWindowEvent::FocusLost() :
                        Application::FWindowEvent::FocusGained());
                    ++FocusStep;
                    return;
                }
                ObservedMinimized = false;
                ObservedRestored = false;
                ActionsSucceeded &= Window.Minimize() == Application::EApplicationResult::Success;
                MinimizeStarted = std::chrono::steady_clock::now(); Stage = 2;
            }
            else if (Stage == 2)
            {
                ObservedMinimized |= Window.IsMinimized();
                if (ObservedMinimized && std::chrono::steady_clock::now() - MinimizeStarted > std::chrono::milliseconds(150))
                {
                    ActionsSucceeded &= Window.Restore() == Application::EApplicationResult::Success;
                    PresentedAtRestore = Presented;
                    Stage = 3;
                }
            }
            else if (Stage == 3 && !Window.IsMinimized() && Window.HasDrawableArea())
            {
                ObservedRestored = true;
                if (Presented >= CycleStartPresented + 8 && Presented >= PresentedAtRestore + 2)
                {
                    ++Cycle;
                    if (Cycle == 3)
                    {
                        ActionsSucceeded &= Window.RequestClose() == Application::EApplicationResult::Success;
                        Stage = 4;
                    }
                    else { CycleStartPresented = Presented; Stage = 0; FocusStep = 0; }
                }
            }
        }, [&](Application::FInteractiveLabSession& Session,Core::uint32 Presented) {
            const auto* Effective = Session.GetEffectiveSettings();
            if (SettingsEdited || Presented < 2 || !Effective || Effective->DisplayGeneration != Session.GetDisplayState().DisplayGeneration ||
                (Session.GetState() != Application::EInteractiveLabSessionState::Ready &&
                 Session.GetState() != Application::EInteractiveLabSessionState::Running)) return;
            auto Edit = *Effective; Edit.CameraRevision = Session.GetCameraState().CameraRevision;
            Edit.ExposureStops = 3; Edit.SdrToneMapVersion = "Sdr.NarkowiczAcesFit.v1";
            SettingsEdited = Session.RequestSettings(Edit);
            if (SettingsEdited)
            {
                Edit.ExposureStops = 100;
                ActionsSucceeded &= !Session.RequestSettings(Edit) && Session.GetPendingSettings() &&
                    Session.GetPendingSettings()->ExposureStops == 3;
            }
        });
    std::cout << "[INFO] live edit=" << SettingsEdited << " revision=" << Result.LastRecordedSettingsRevision
        << " exposure=" << Result.LastRecordedExposureStops << " transform=" << Result.LastRecordedTransformVersion.CStr() << '\n';
    Check(Failed,SettingsEdited && Result.LastRecordedSettingsRevision > 1 && Result.LastRecordedExposureStops == 3 &&
        Result.LastRecordedTransformVersion == "Sdr.NarkowiczAcesFit.v1",
        "native scene frames consume revised exposure and tone map across resize while invalid edits preserve intent");
    std::cout << "[INFO] scene lifecycle exit=" << static_cast<int>(Result.ExitCode)
        << " cycles=" << Cycle << " stage=" << Stage << " actions=" << ActionsSucceeded << " minimized=" << ObservedMinimized
        << " restored=" << ObservedRestored << " failure=" << Result.FirstFailure.CStr() << '\n';
    const auto& Before = Result.BeforeNativeShutdown.RuntimeSnapshot.NativeOperations;
    const auto& After = Result.AfterNativeShutdown.RuntimeSnapshot.NativeOperations;
    const auto& Frames = Result.FinalFrameState;
    const auto& Presentation = Result.AfterNativeShutdown.RuntimeSnapshot.NativePresentation;
    const auto& Status = Result.BeforeNativeShutdown;
    std::cout << "[INFO] scene capability: adapter=" << Status.RuntimeSnapshot.AdapterName.CStr()
        << " mode=" << static_cast<int>(Status.RetirementMode)
        << " reason=" << static_cast<int>(Status.RetirementReason)
        << " optional-advertised=" << Status.Capabilities.bOptionalPresentationFenceAdvertised
        << " optional-enabled=" << Status.Capabilities.bOptionalPresentationFenceEnabled
        << " peak-color-bytes=" << Presentation.PeakEstimatedColorBytes << '\n';
    Check(Failed, Before.bAvailable && After.bAvailable && After.RetainedSubmissionOwnerCount == 0 &&
        Before.ImageReadbackCopyCount == 0 &&
        Before.ReadbackMapCount == 0 && Before.ReadbackWaitCount == 0 &&
        Before.QueueIdleCallCount == 0 && Before.DeviceIdleCallCount == 0 &&
        Frames.SubmittedFrameCount == Result.SubmittedFrames &&
        Frames.RenderCompletedFrameCount == Result.RenderCompletedFrames &&
        Frames.RenderRetiredFrameCount == Result.SubmittedFrames &&
        Frames.BusySlotCount == 0 && Frames.ActiveAttachmentBytes == 0 &&
        Frames.RetainedPresentationCount == 0 && Presentation.PendingAcquireCount == 0 &&
        Presentation.AcquisitionRecordCount == 0 && Presentation.PresentationOwnerCount == 0 &&
        Presentation.ActiveGeneration == 0 && Presentation.RetiringGeneration == 0 &&
        Presentation.ResidualNativeOwners == 0 && Presentation.AbandonedNativeOwners == 0 &&
        Presentation.PeakEstimatedColorBytes > 0 && Presentation.PeakEstimatedColorBytes <= 512ULL * 1024 * 1024 &&
        Frames.ProvenPresentationReleaseCount <= After.ProvenPresentationReleaseCount,
        "real scene lifecycle has zero native readbacks/idles and separately counted render/presentation retirement");
    Check(Failed, Result.ExitCode == Demo::EDemoExitCode::Success && Result.PresentedFrames >= 24 && Stage == 4 && Cycle == 3 &&
        Result.RenderCompletedFrames >= Result.PresentedFrames && ActionsSucceeded && ObservedMinimized && ObservedRestored &&
        Result.FirstFailure.IsEmpty() &&
        Result.ShutdownAssurance == (Status.RetirementMode == ERHIPresentationRetirementMode::AcquireHistory
            ? ERHIShutdownAssurance::IdleAssumed : ERHIShutdownAssurance::Proven),
        "strict cooked scene lab resumes current-drawable rendering after resize/minimize and terminates with qualified native cleanup");
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
    RunSceneLifecycle(Failed);
    return Failed == 0 ? 0 : 1;
}
