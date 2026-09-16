#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanDevice.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
using namespace Stoner;
using namespace Stoner::RHI;
using namespace Stoner::Backend::Vulkan;

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

[[maybe_unused]] void RunCase(int& Failed, bool ForceAcquireHistory)
{
    Application::FWindow Window;
    Application::FWindowDesc WindowDesc;
    WindowDesc.Title = ForceAcquireHistory
        ? "Stoner Vulkan Borrowed Acquire History"
        : "Stoner Vulkan Borrowed Auto";
    WindowDesc.ClientWidth = 160;
    WindowDesc.ClientHeight = 96;
    WindowDesc.bVisible = true;
    const bool WindowReady = Window.CreateRealWindow(WindowDesc) ==
        Application::EApplicationResult::Success;
    FVulkanDevice Device;
    FVulkanInstanceDesc DeviceDesc;
    DeviceDesc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    DeviceDesc.bRequestValidation = false;
    const bool DeviceReady = WindowReady &&
        Device.Initialize(DeviceDesc) == ERHIResult::Success &&
        Device.EnableNativeLabPresentationRuntime(
            Window.GetPlatformWindow(), ForceAcquireHistory) == ERHIResult::Success;
    Check(Failed, DeviceReady, "borrowed Vulkan probe creates a real lab device");
    if (!DeviceReady)
    {
        (void)Device.Shutdown();
        (void)Window.Destroy();
        return;
    }

    FRHIPresentationSurfaceDesc SurfaceDesc;
    SurfaceDesc.Window = Window.GetPlatformWindow();
    const auto Surface = Device.CreatePresentationSurface(SurfaceDesc);
    FRHIPresentationCapabilities Capabilities;
    FRHIPresentationFormatColorSpacePair Pair;
    if (Surface.Succeeded() && Surface.Object->QueryCapabilities(Capabilities) ==
        ERHIResult::Success)
    {
        for (const auto& Candidate : Capabilities.SupportedPairs)
        {
            if (Candidate.ColorSpace == ERHIPresentationColorSpace::SrgbNonlinear &&
                (Candidate.Format == ERHIFormat::B8G8R8A8_UNorm ||
                 Candidate.Format == ERHIFormat::R8G8B8A8_UNorm))
            {
                Pair = Candidate;
                break;
            }
        }
    }
    FRHISwapchainDesc Request;
    Request.Width = Window.GetDrawableWidth();
    Request.Height = Window.GetDrawableHeight();
    Request.FramesInFlight = 2;
    Request.PreferredFormat = Pair.Format;
    Request.PreferredColorSpace = Pair.ColorSpace;
    Request.SurfaceCapabilityGeneration = Capabilities.CapabilityGeneration;
    const auto Swapchain = Pair.IsValid()
        ? Device.CreateSwapchain(Surface.Object, Request)
        : TRHIObjectResult<IRHISwapchain>{};
    const auto Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    Check(Failed, Swapchain.Succeeded() && Queue.Succeeded(),
        "borrowed Vulkan probe creates a native swapchain and queue");

    FRHIPresentationCapabilities PublishedCapabilities;
    const bool QueriedPublished = Surface.Succeeded() &&
        Surface.Object->QueryCapabilities(PublishedCapabilities) == ERHIResult::Success;
    const auto ExpectedMode = !ForceAcquireHistory &&
        PublishedCapabilities.bOptionalPresentationFenceEnabled
        ? ERHIPresentationRetirementMode::PresentationFence
        : ERHIPresentationRetirementMode::AcquireHistory;
    Check(Failed, QueriedPublished && Swapchain.Succeeded() &&
        PublishedCapabilities.PresentationRetirementMode == ExpectedMode &&
        PublishedCapabilities.PresentationRetirementReason !=
            ERHIPresentationRetirementReason::Unknown &&
        PublishedCapabilities.bSupportsIndependentPresentationCompletion ==
            (ExpectedMode == ERHIPresentationRetirementMode::PresentationFence),
        "created borrowed swapchain publishes its actual retirement mode and reason");
    std::cout << "[INFO] borrowed forceAcquireHistory=" << ForceAcquireHistory
              << " mode=" << static_cast<int>(PublishedCapabilities.PresentationRetirementMode)
              << " reason=" << static_cast<int>(PublishedCapabilities.PresentationRetirementReason)
              << " optionalEnabled=" << PublishedCapabilities.bOptionalPresentationFenceEnabled
              << '\n';

    std::array<Core::TSharedPtr<IRHIFence>, 2> SlotFences;
    std::vector<FRHIPresentationLease> PresentationLeases;
    bool FramesPassed = Swapchain.Succeeded() && Queue.Succeeded();
    Core::uint32 CompletedFrames = 0;
    bool NegativeChecksDone = false;
    for (Core::uint32 Index = 0; FramesPassed && Index < 12; ++Index)
    {
        const Core::uint32 Slot = Index % 2;
        if (SlotFences[Slot])
        {
            FramesPassed = PollBounded(Window, [&] {
                return SlotFences[Slot]->Wait(0);
            }) == ERHIResult::Success;
            if (!FramesPassed) break;
        }
        FRHIBorrowedAcquiredTarget Target;
        const auto Acquire = PollBounded(Window, [&] {
            return Swapchain.Object->AcquireBorrowedTarget(
                300000 + Index, Slot, Target);
        });
        FramesPassed = Acquire == ERHIResult::Success && Target.IsValid() &&
            Target.AcquireSemaphore && Target.Frame.FrameToken == 300000 + Index &&
            Target.FrameSlotIndex == Slot;
        if (!FramesPassed)
        {
            std::cerr << "borrowed acquire frame=" << Index
                      << " result=" << static_cast<int>(Acquire) << '\n';
            break;
        }

        FRHIRenderPassDesc PassDesc;
        PassDesc.Attachments = {{ERHIAttachmentRole::Color, Target.Frame.Format,
            ERHISampleCount::One, ERHIAttachmentLoadOp::Clear,
            ERHIAttachmentStoreOp::Store}};
        const auto Pass = Device.CreateRenderPass(PassDesc);
        FRHIFramebufferDesc FramebufferDesc;
        FramebufferDesc.RenderPass = Pass.Object;
        FramebufferDesc.Attachments = {{Target.Texture}};
        FramebufferDesc.Width = Target.Frame.Width;
        FramebufferDesc.Height = Target.Frame.Height;
        const auto Framebuffer = Device.CreateFramebuffer(FramebufferDesc);
        const auto Command = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
        const auto RenderFence = Device.CreateFence(false);
        const bool UseTypedRenderLease = Index == 0;
        Core::TSharedPtr<IRHISemaphore> Ready;
        if (!UseTypedRenderLease)
        {
            const auto ReadyResult = Device.CreateSemaphore();
            if (ReadyResult.Succeeded())
                Ready = ReadyResult.Object;
        }
        FramesPassed = Pass.Succeeded() && Framebuffer.Succeeded() &&
            Command.Succeeded() && RenderFence.Succeeded() &&
            (UseTypedRenderLease || Ready != nullptr);
        if (!FramesPassed)
        {
            (void)Swapchain.Object->ReleaseBorrowedTarget(Target, nullptr);
            break;
        }
        if (!NegativeChecksDone && !UseTypedRenderLease)
        {
            FRHIBorrowedAcquiredTarget StaleTarget = Target;
            ++StaleTarget.Frame.FrameToken;
            FRHIPresentationLease StaleLease;
            const auto StalePresent =
                Swapchain.Object->PresentBorrowedTarget(
                    StaleTarget, Ready, StaleLease);
            Check(Failed,
                StalePresent == ERHIResult::InvalidState &&
                    !StaleLease.IsValid(),
                "stale borrowed frame identity is rejected before presentation");
            NegativeChecksDone = true;
        }
        FRHIRenderPassClearValues Clear;
        Clear.Colors = {{0.05f, 0.15f, 0.25f, 1.0f}};
        FRHIResourceBarrierDesc ToPresent;
        ToPresent.Texture = Target.Texture;
        ToPresent.RequiredTextureUsage = ERHITextureUsage::Present;
        ToPresent.Before = ERHIResourceLayout::ColorAttachment;
        ToPresent.After = ERHIResourceLayout::Present;
        FramesPassed = Command.Object->Begin() == ERHIResult::Success &&
            Command.Object->BeginRenderPass(Pass.Object, Framebuffer.Object, Clear) ==
                ERHIResult::Success &&
            Command.Object->EndRenderPass() == ERHIResult::Success &&
            Command.Object->RecordLayoutTransition(ToPresent) == ERHIResult::Success &&
            Command.Object->End() == ERHIResult::Success;
        if (FramesPassed && Index == 0)
        {
            const auto GenericCommand = Device.CreateCommandBuffer(
                ERHIQueueType::Graphics);
            const bool GenericRecorded = GenericCommand.Succeeded() &&
                GenericCommand.Object->Begin() == ERHIResult::Success &&
                GenericCommand.Object->RecordBarrier() == ERHIResult::Success &&
                GenericCommand.Object->End() == ERHIResult::Success;
            const auto GenericSubmit = GenericRecorded
                ? Queue.Object->Submit(GenericCommand.Object,
                    {Target.AcquireSemaphore}, {}, nullptr)
                : ERHIResult::Failed;
            Check(Failed, GenericSubmit == ERHIResult::InvalidState &&
                Target.AcquireSemaphore->IsSignaled(),
                "ordinary submit cannot consume a lab acquire semaphore for unrelated commands");
            const auto MissingAcquire = Queue.Object->SubmitDeferred(
                Command.Object, {}, {}, RenderFence.Object);
            Check(Failed, MissingAcquire == ERHIResult::InvalidState,
                "borrowed submit rejects the generic overload without its acquire synchronization");
            const auto WrongAcquire = Device.CreateSemaphore();
            const auto WrongWait = WrongAcquire.Succeeded()
                ? Queue.Object->SubmitDeferred(Command.Object,
                    {WrongAcquire.Object}, {}, RenderFence.Object)
                : ERHIResult::Failed;
            Check(Failed, WrongWait == ERHIResult::InvalidState,
                "borrowed submit rejects a same-device semaphore that is not its acquire owner");
            FVulkanDevice ForeignDevice;
            const bool ForeignReady =
                ForeignDevice.Initialize(DeviceDesc) == ERHIResult::Success;
            const auto ForeignSignal = ForeignReady
                ? ForeignDevice.CreateSemaphore()
                : TRHIObjectResult<IRHISemaphore>{};
            const auto ForeignSubmit = ForeignSignal.Succeeded()
                ? Queue.Object->SubmitDeferred(Command.Object,
                    {Target.AcquireSemaphore}, {ForeignSignal.Object},
                    RenderFence.Object)
                : ERHIResult::Failed;
            Check(Failed, ForeignSubmit == ERHIResult::InvalidState,
                "borrowed submit rejects a foreign-device render semaphore before native submission");
            (void)ForeignDevice.Shutdown();
        }
        const auto Submit = FramesPassed
            ? (UseTypedRenderLease
                ? Queue.Object->SubmitDeferred(Command.Object,
                    {Target.AcquireSemaphore}, {}, RenderFence.Object)
                : Queue.Object->SubmitDeferred(Command.Object,
                    {Target.AcquireSemaphore}, {Ready}, RenderFence.Object))
            : ERHIResult::InvalidState;
        if (Submit != ERHIResult::Success)
        {
            std::cerr << "borrowed submit frame=" << Index
                      << " result=" << static_cast<int>(Submit) << '\n';
            FramesPassed = false;
            (void)Swapchain.Object->ReleaseBorrowedTarget(Target, nullptr);
            break;
        }
        SlotFences[Slot] = RenderFence.Object;
        FRHIPresentationLease Lease;
        ERHIResult Present = ERHIResult::InvalidState;
        if (UseTypedRenderLease)
        {
            const auto RenderComplete = PollBounded(Window, [&] {
                return RenderFence.Object->Wait(0);
            });
            FRHIRenderLease RenderLease;
            RenderLease.Frame = Target.Frame;
            RenderLease.FrameSlotIndex = Slot;
            RenderLease.CompletionFence = RenderFence.Object;
            if (RenderComplete == ERHIResult::Success)
            {
                FRHIBorrowedAcquiredTarget ForgedTarget = Target;
                ForgedTarget.Frame.ImageIndex =
                    (Target.Frame.ImageIndex + 1) % Swapchain.Object->GetFrameCount();
                FRHIRenderLease ForgedRenderLease = RenderLease;
                ForgedRenderLease.Frame = ForgedTarget.Frame;
                FRHIPresentationLease ForgedLease;
                Check(Failed, Swapchain.Object->PresentBorrowedTarget(
                    ForgedTarget, ForgedRenderLease, ForgedLease) ==
                        ERHIResult::InvalidState && !ForgedLease.IsValid(),
                    "forged image identity cannot publish a lease for another native target");
            }
            Present = RenderComplete == ERHIResult::Success
                ? Swapchain.Object->PresentBorrowedTarget(
                    Target, RenderLease, Lease)
                : RenderComplete;
        }
        else
        {
            Present = Swapchain.Object->PresentBorrowedTarget(
                Target, Ready, Lease);
        }
        FramesPassed = Present == ERHIResult::Success && Lease.Matches(Target) &&
            Lease.PresentationCompletionFence != RenderFence.Object;
        if (UseTypedRenderLease)
        {
            Check(Failed,
                Present == ERHIResult::Success && Lease.Matches(Target),
                "typed render lease presents after finite render completion");
        }
        if (!FramesPassed)
        {
            std::cerr << "borrowed present frame=" << Index
                      << " result=" << static_cast<int>(Present) << '\n';
            break;
        }
        if (UseTypedRenderLease)
        {
            FRHIRenderLease DuplicateRenderLease;
            DuplicateRenderLease.Frame = Target.Frame;
            DuplicateRenderLease.FrameSlotIndex = Slot;
            DuplicateRenderLease.CompletionFence = RenderFence.Object;
            FRHIPresentationLease DuplicateLease;
            const auto DuplicatePresent =
                Swapchain.Object->PresentBorrowedTarget(
                    Target, DuplicateRenderLease, DuplicateLease);
            Check(Failed, DuplicatePresent == ERHIResult::InvalidState &&
                !DuplicateLease.IsValid(),
                "duplicate typed render lease is rejected without a retryable pending result");
            Check(Failed, Swapchain.Object->ReleaseBorrowedTarget(Target, RenderFence.Object) ==
                ERHIResult::InvalidState,
                "successfully queued presentation cannot be cancelled as an unpresented frame");
        }
        if (!UseTypedRenderLease && Index == 1)
        {
            FRHIPresentationLease DuplicateLease;
            const auto DuplicatePresent =
                Swapchain.Object->PresentBorrowedTarget(
                    Target, Ready, DuplicateLease);
            Check(Failed,
                DuplicatePresent == ERHIResult::InvalidState &&
                    !DuplicateLease.IsValid(),
                "duplicate present of one borrowed target is rejected");
        }
        PresentationLeases.push_back(Lease);
        ++CompletedFrames;
        // Polling a positive presentation lease is distinct from observing
        // the slot's render fence. No image readback/copy/map occurs here.
        for (const auto& Prior : PresentationLeases)
            (void)Prior.PresentationCompletionFence->IsSignaled();
    }
    Check(Failed, FramesPassed && CompletedFrames == 12,
        "twelve real borrowed clears submit and present through two render slots");
    bool RenderDrained = true;
    for (const auto& Fence : SlotFences)
    {
        if (Fence && PollBounded(Window, [&] { return Fence->Wait(0); }) !=
            ERHIResult::Success)
            RenderDrained = false;
    }
    Check(Failed, RenderDrained, "borrowed render fences complete within finite bounds");
    bool HasPositiveRelease = false;
    for (const auto& Lease : PresentationLeases)
        HasPositiveRelease |= Lease.PresentationCompletionFence->IsSignaled();
    Check(Failed, CompletedFrames == 12 && HasPositiveRelease,
        "native presentation yields positive release evidence before terminal cleanup");
    if (Swapchain.Succeeded() && CompletedFrames == 12 && RenderDrained)
    {
        FRHISwapchainDesc PausedRequest = Request;
        PausedRequest.Width = 0;
        PausedRequest.Height = 0;
        const auto Pause = Swapchain.Object->Reconfigure(PausedRequest);
        FRHIBorrowedAcquiredTarget PausedTarget;
        const auto PausedAcquire = Swapchain.Object->AcquireBorrowedTarget(
            399998, 0, PausedTarget);
        Check(Failed, Pause == ERHIResult::NotReady &&
            PausedAcquire == ERHIResult::NotReady && !PausedTarget.IsValid(),
            "zero extent pauses borrowed acquisition without publishing a target");
        const auto Resume = PollBounded(Window, [&] {
            return Swapchain.Object->Reconfigure(Request);
        });
        Check(Failed, Resume == ERHIResult::Success &&
            Swapchain.Object->GetResolvedPresentationState().IsValid(),
            "nonzero reconfigure resumes within a finite retry bound");
    }
    FRHIBorrowedAcquiredTarget CanceledTarget;
    const auto CanceledAcquire = PollBounded(Window, [&] {
        return Swapchain.Succeeded()
            ? Swapchain.Object->AcquireBorrowedTarget(399999, 0, CanceledTarget)
            : ERHIResult::InvalidState;
    });
    const bool CanceledTargetReady =
        CanceledAcquire == ERHIResult::Success && CanceledTarget.IsValid();
    Check(Failed, CanceledTargetReady,
        "borrowed API can acquire an additional target for cancellation coverage");
    const auto CanceledCommand = Device.CreateCommandBuffer(
        ERHIQueueType::Graphics);
    const auto CanceledFence = Device.CreateFence(false);
    FRHIResourceBarrierDesc CanceledBarrier;
    CanceledBarrier.Texture = CanceledTarget.Texture;
    CanceledBarrier.RequiredTextureUsage = ERHITextureUsage::Present;
    CanceledBarrier.Before = ERHIResourceLayout::Undefined;
    CanceledBarrier.After = ERHIResourceLayout::Present;
    const bool Recorded = CanceledTargetReady && CanceledCommand.Succeeded() &&
        CanceledFence.Succeeded() &&
        CanceledCommand.Object->Begin() == ERHIResult::Success &&
        CanceledCommand.Object->RecordLayoutTransition(CanceledBarrier) ==
            ERHIResult::Success &&
        CanceledCommand.Object->End() == ERHIResult::Success;
    if (CanceledTargetReady && !ForceAcquireHistory)
    {
        Check(Failed, CanceledTarget.Texture->Invalidate() == ERHIResult::Success,
            "borrowed wrapper invalidation preserves the independently owned acquisition");
    }
    const auto CanceledRelease = CanceledTargetReady
        ? Swapchain.Object->ReleaseBorrowedTarget(CanceledTarget, nullptr)
        : ERHIResult::InvalidState;
    Check(Failed,
        CanceledRelease == ERHIResult::Success,
        "unsubmitted borrowed target accepts logical cancellation before terminal cleanup");
    if (CanceledRelease == ERHIResult::Success)
    {
        FRHIBorrowedAcquiredTarget CanceledRetry;
        Check(Failed, Swapchain.Object->AcquireBorrowedTarget(
            399999, 0, CanceledRetry) == ERHIResult::InvalidState &&
            !CanceledRetry.IsValid(),
            "logical cancellation does not republish the retained native target");
        const auto CanceledSubmit = Recorded
            ? Queue.Object->SubmitDeferred(CanceledCommand.Object,
                {CanceledTarget.AcquireSemaphore}, {}, CanceledFence.Object)
            : ERHIResult::Failed;
        Check(Failed, CanceledSubmit == ERHIResult::InvalidState,
            "logical cancellation rejects later native render submission");
    }
    std::vector<Core::TSharedPtr<IRHIFence>> ProvenBeforeShutdown;
    for (const auto& Lease : PresentationLeases)
    {
        if (Lease.PresentationCompletionFence->IsSignaled())
            ProvenBeforeShutdown.push_back(Lease.PresentationCompletionFence);
    }
    const auto Shutdown = Device.Shutdown();
    Check(Failed, Shutdown == ERHIResult::Success &&
        Device.GetRuntimeSnapshot().GetTotalLiveObjectCount() == 0,
        "borrowed Vulkan device shutdown retires native owners");
    bool CompletedLeasesSurvive = !ProvenBeforeShutdown.empty();
    for (const auto& Fence : ProvenBeforeShutdown)
        CompletedLeasesSurvive &= Fence->Wait(0) == ERHIResult::Success;
    Check(Failed, CompletedLeasesSurvive,
        "proven presentation leases remain queryable after device shutdown");
    (void)Window.Destroy();
}
} // namespace

int RunVulkanLabBorrowedNativeTests()
{
    int Failed = 0;
    const char* Required = std::getenv("STONER_REQUIRE_VULKAN_LAB_BORROWED");
    if (!Required || std::string_view(Required) != "1")
    {
        std::cout << "[SKIP] native borrowed Vulkan window test requires explicit opt-in\n";
        return 0;
    }
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && \
    defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    RunCase(Failed, false);
    RunCase(Failed, true);
#else
    Check(Failed, false, "required borrowed Vulkan test needs native Vulkan and GLFW");
#endif
    return Failed;
}
