#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "RHI/RHIMinimal.h"

#if SG_PLATFORM_MAC
#include "../../Source/Backend/Metal/Private/FMetalPresentationSurface.h"
#endif

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    #define GLFW_INCLUDE_NONE
    #include <GLFW/glfw3.h>
#endif

#include <charconv>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace
{

using namespace Stoner;
using namespace Stoner::Application;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FOptions
{
    uint32 Frames = 3000;
    uint32 LifecycleCycles = 20;
    FString ReportPath;
    bool bBorrowedPreview = false;
};

bool ParseCount(const char* Text, uint32& Out)
{
    const std::string Value = Text ? Text : "";
    const auto Parsed = std::from_chars(
        Value.data(), Value.data() + Value.size(), Out);
    return Parsed.ec == std::errc{} &&
        Parsed.ptr == Value.data() + Value.size() && Out > 0;
}

bool ParseOptions(int ArgCount, char** Arguments, FOptions& Out)
{
    for (int Index = 1; Index < ArgCount; ++Index)
    {
        const std::string Argument = Arguments[Index];
        if (Argument == "--frames" || Argument == "--cycles" ||
            Argument == "--report")
        {
            if (++Index >= ArgCount) return false;
        if (Argument == "--report") Out.ReportPath = Arguments[Index];
            else if (Argument == "--frames" &&
                !ParseCount(Arguments[Index], Out.Frames))
                return false;
            else if (Argument == "--cycles")
            {
                const std::string Value = Arguments[Index];
                const auto Parsed = std::from_chars(
                    Value.data(), Value.data() + Value.size(),
                    Out.LifecycleCycles);
                if (Parsed.ec != std::errc{} ||
                    Parsed.ptr != Value.data() + Value.size())
                    return false;
            }
        }
        else if (Argument == "--borrowed-preview")
        {
            Out.bBorrowedPreview = true;
        }
        else return false;
    }
    return Out.Frames >= Out.LifecycleCycles;
}

void WriteReport(const FOptions& Options, uint32 Presented, uint32 Cycles,
    const char* Result, const char* Failure, bool bLayerDetached = false,
    bool bDeviceShutdown = false, bool bWindowDestroyed = false,
    bool bOwnershipClean = false, bool bPresentationLeasesReleased = true)
{
    if (Options.ReportPath.IsEmpty()) return;
    std::ofstream Output(std::string(Options.ReportPath.View()),
        std::ios::binary | std::ios::trunc);
    if (!Output) return;
    Output << "{\n"
        << "  \"schema\": \"stoner.metal.presentation-probe.v1\",\n"
        << "  \"requestedFrames\": " << Options.Frames << ",\n"
        << "  \"mode\": \""
        << (Options.bBorrowedPreview ? "borrowed-preview" : "formal")
        << "\",\n"
        << "  \"presentedFrames\": " << Presented << ",\n";
    if (Options.bBorrowedPreview)
    {
        Output << "  \"presentedFramesMeaning\": \""
            << "accepted presentation submissions; not physical scanout\",\n"
            << "  \"presentationLeasesReleased\": "
            << (bPresentationLeasesReleased ? "true" : "false") << ",\n";
    }
    Output << "  \"requestedLifecycleCycles\": "
        << Options.LifecycleCycles << ",\n"
        << "  \"completedLifecycleCycles\": " << Cycles << ",\n"
        << "  \"layerDetached\": "
        << (bLayerDetached ? "true" : "false") << ",\n"
        << "  \"deviceShutdown\": "
        << (bDeviceShutdown ? "true" : "false") << ",\n"
        << "  \"windowDestroyed\": "
        << (bWindowDestroyed ? "true" : "false") << ",\n"
        << "  \"ownershipClean\": "
        << (bOwnershipClean ? "true" : "false") << ",\n"
        << "  \"result\": \"" << Result << "\",\n"
        << "  \"failure\": \"" << Failure << "\"\n"
        << "}\n";
}

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
bool WaitForWindowAttribute(GLFWwindow* Window, int Attribute, int Expected)
{
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    do
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(Window, Attribute) == Expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (std::chrono::steady_clock::now() < Deadline);
    return false;
}

bool WaitForDrawableExtent(
    GLFWwindow* Window, int ExpectedLogicalWidth, int ExpectedLogicalHeight)
{
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    do
    {
        glfwPollEvents();
        int LogicalWidth = 0;
        int LogicalHeight = 0;
        int Width = 0;
        int Height = 0;
        glfwGetWindowSize(Window, &LogicalWidth, &LogicalHeight);
        glfwGetFramebufferSize(Window, &Width, &Height);
        if (LogicalWidth == ExpectedLogicalWidth &&
            LogicalHeight == ExpectedLogicalHeight && Width > 0 && Height > 0 &&
            glfwGetWindowAttrib(Window, GLFW_ICONIFIED) == GLFW_FALSE)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (std::chrono::steady_clock::now() < Deadline);
    return false;
}

ERHIResult WaitForRenderCompletion(
    const TSharedPtr<IRHIFence>& RenderFence,
    GLFWwindow* Window,
    std::chrono::milliseconds Timeout)
{
    if (!RenderFence) return ERHIResult::InvalidState;
    const auto Deadline = std::chrono::steady_clock::now() + Timeout;
    for (;;)
    {
        if (Window) glfwPollEvents();
        const ERHIResult Result = RenderFence->Wait(0);
        if (Result != ERHIResult::NotReady) return Result;
        if (std::chrono::steady_clock::now() >= Deadline)
            return ERHIResult::Timeout;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool IsRetryablePresentationResult(ERHIResult Result) noexcept
{
    return Result == ERHIResult::Unavailable ||
        Result == ERHIResult::NotReady ||
        Result == ERHIResult::ResizeRequired;
}

void LatchTerminalFailure(
    bool& bTerminalFailure, const char*& FailureReason, const char* Reason)
{
    if (!bTerminalFailure)
    {
        bTerminalFailure = true;
        FailureReason = Reason;
    }
}

bool RenderClearFrame(const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHICommandQueue>& Queue,
    const TSharedPtr<IRHISwapchain>& Swapchain)
{
    uint32 FrameIndex = 0;
    const auto Acquire = Swapchain->AcquireNextFrame(FrameIndex);
    if (Acquire == ERHIResult::Unavailable ||
        Acquire == ERHIResult::NotReady ||
        Acquire == ERHIResult::ResizeRequired)
        return false;
    if (Acquire != ERHIResult::Success) return false;
    const auto Image = Swapchain->GetImage(FrameIndex);
    if (!Image) return false;

    FRHIRenderPassDesc PassDesc;
    PassDesc.Attachments.push_back({ERHIAttachmentRole::Color,
        Image->GetFormat(), ERHISampleCount::One,
        ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store});
    const auto Pass = Device->CreateRenderPass(PassDesc);
    FRHIFramebufferDesc FramebufferDesc;
    FramebufferDesc.RenderPass = Pass.Object;
    FramebufferDesc.Attachments.push_back({Image, 0, 0});
    FramebufferDesc.Width = Image->GetDesc().Width;
    FramebufferDesc.Height = Image->GetDesc().Height;
    const auto Framebuffer = Pass.Succeeded()
        ? Device->CreateFramebuffer(FramebufferDesc)
        : TRHIObjectResult<IRHIFramebuffer>{};
    const auto Commands = Device->CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto Ready = Device->CreateSemaphore();
    if (!Pass.Succeeded() || !Framebuffer.Succeeded() ||
        !Commands.Succeeded() || !Ready.Succeeded())
        return false;
    FRHIRenderPassClearValues Clears;
    Clears.Colors.push_back({0.08f, 0.32f, 0.65f, 1.0f});
    if (Commands.Object->Begin() != ERHIResult::Success ||
        Commands.Object->BeginRenderPass(
            Pass.Object, Framebuffer.Object, Clears) != ERHIResult::Success ||
        Commands.Object->EndRenderPass() != ERHIResult::Success ||
        Commands.Object->End() != ERHIResult::Success ||
        Queue->Submit(Commands.Object, {}, {Ready.Object}, nullptr) !=
            ERHIResult::Success ||
        Swapchain->Present(FrameIndex, Ready.Object) != ERHIResult::Success)
        return false;
    return true;
}

bool RetireBorrowedLeases(
    TArray<FRHIPresentationLease>& Leases,
    bool& bTerminalFailure, const char*& FailureReason)
{
    bool bAllHealthy = true;
    for (auto It = Leases.begin(); It != Leases.end();)
    {
        if (!It->PresentationCompletionFence)
        {
            bAllHealthy = false;
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "presentation-fence");
            It = Leases.erase(It);
            continue;
        }
        const ERHIResult Result = It->PresentationCompletionFence->Wait(0);
        if (Result == ERHIResult::Success)
            It = Leases.erase(It);
        else if (Result == ERHIResult::NotReady)
            ++It;
        else
        {
            bAllHealthy = false;
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "presentation-fence");
            ++It;
        }
    }
    return bAllHealthy;
}

bool DrainBorrowedLeases(
    TArray<FRHIPresentationLease>& Leases,
    GLFWwindow* Window,
    std::chrono::milliseconds Timeout,
    bool& bTerminalFailure, const char*& FailureReason)
{
    const auto Deadline = std::chrono::steady_clock::now() + Timeout;
    while (!Leases.empty() && std::chrono::steady_clock::now() < Deadline)
    {
        if (Window) glfwPollEvents();
        if (!RetireBorrowedLeases(
                Leases, bTerminalFailure, FailureReason))
            return false;
        if (Leases.empty()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Leases.empty())
        LatchTerminalFailure(
            bTerminalFailure, FailureReason, "presentation-drain-timeout");
    return Leases.empty();
}

bool RenderBorrowedFrame(const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHICommandQueue>& Queue,
    const TSharedPtr<IRHISwapchain>& Swapchain,
    uint64 FrameToken,
    uint32 FrameSlot,
    TArray<FRHIPresentationLease>& PresentationLeases,
    GLFWwindow* Window,
    bool& bTerminalFailure, const char*& FailureReason)
{
    if (!RetireBorrowedLeases(
            PresentationLeases, bTerminalFailure, FailureReason))
        return false;
    if (PresentationLeases.size() >= MaxRHIPresentationImageLeases)
        return false;
    const bool bUseTypedRenderLease = (FrameToken % 2) == 0;
    FRHIBorrowedAcquiredTarget Target;
    const ERHIResult Acquire = Swapchain->AcquireBorrowedTarget(
        FrameToken, FrameSlot, Target);
    if (Acquire != ERHIResult::Success)
    {
        if (!IsRetryablePresentationResult(Acquire))
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-acquire");
        return false;
    }

    const auto Image = Target.Texture;
    if (!Target.IsValid() || !Image)
    {
        const ERHIResult ReleaseResult =
            Swapchain->ReleaseBorrowedTarget(Target, nullptr);
        if (ReleaseResult != ERHIResult::Success)
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-release");
        LatchTerminalFailure(
            bTerminalFailure, FailureReason, "borrowed-target");
        return false;
    }
    FRHIRenderPassDesc PassDesc;
    PassDesc.Attachments.push_back({ERHIAttachmentRole::Color,
        Image->GetFormat(), ERHISampleCount::One,
        ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store});
    const auto Pass = Device->CreateRenderPass(PassDesc);
    FRHIFramebufferDesc FramebufferDesc;
    FramebufferDesc.RenderPass = Pass.Object;
    FramebufferDesc.Attachments.push_back({Image, 0, 0});
    FramebufferDesc.Width = Image->GetDesc().Width;
    FramebufferDesc.Height = Image->GetDesc().Height;
    const auto Framebuffer = Pass.Succeeded()
        ? Device->CreateFramebuffer(FramebufferDesc)
        : TRHIObjectResult<IRHIFramebuffer>{};
    const auto Commands = Device->CreateCommandBuffer(ERHIQueueType::Graphics);
    TSharedPtr<IRHISemaphore> Ready;
    if (!bUseTypedRenderLease)
    {
        const auto ReadyResult = Device->CreateSemaphore();
        if (ReadyResult.Succeeded()) Ready = ReadyResult.Object;
    }
    const auto RenderFence = Device->CreateFence(false);
    if (!Pass.Succeeded() || !Framebuffer.Succeeded() ||
        !Commands.Succeeded() || (!bUseTypedRenderLease && !Ready) ||
        !RenderFence.Succeeded())
    {
        const ERHIResult ReleaseResult =
            Swapchain->ReleaseBorrowedTarget(Target, nullptr);
        if (ReleaseResult != ERHIResult::Success)
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-release");
        LatchTerminalFailure(
            bTerminalFailure, FailureReason, "borrowed-resource");
        return false;
    }
    FRHIRenderPassClearValues Clears;
    Clears.Colors.push_back({0.08f, 0.32f, 0.65f, 1.0f});
    if (Commands.Object->Begin() != ERHIResult::Success ||
        Commands.Object->BeginRenderPass(
            Pass.Object, Framebuffer.Object, Clears) != ERHIResult::Success ||
        Commands.Object->EndRenderPass() != ERHIResult::Success ||
        Commands.Object->End() != ERHIResult::Success)
    {
        const ERHIResult ReleaseResult =
            Swapchain->ReleaseBorrowedTarget(Target, nullptr);
        if (ReleaseResult != ERHIResult::Success)
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-release");
        LatchTerminalFailure(
            bTerminalFailure, FailureReason, "borrowed-record");
        return false;
    }
    const ERHIResult Submit = bUseTypedRenderLease
        ? Queue->SubmitDeferred(
            Commands.Object, {}, {}, RenderFence.Object)
        : Queue->SubmitDeferred(
            Commands.Object, {}, {Ready}, RenderFence.Object);
    if (Submit != ERHIResult::Success)
    {
        const ERHIResult ReleaseResult =
            Swapchain->ReleaseBorrowedTarget(Target, nullptr);
        if (ReleaseResult != ERHIResult::Success)
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-release");
        if (!IsRetryablePresentationResult(Submit))
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-submit");
        return false;
    }

    FRHIPresentationLease Lease;
    ERHIResult Present = ERHIResult::InvalidState;
    if (bUseTypedRenderLease)
    {
        const ERHIResult RenderResult = WaitForRenderCompletion(
            RenderFence.Object, Window, std::chrono::seconds(5));
        if (RenderResult != ERHIResult::Success)
        {
            const ERHIResult ReleaseResult =
                Swapchain->ReleaseBorrowedTarget(Target, RenderFence.Object);
            if (ReleaseResult != ERHIResult::Success)
                LatchTerminalFailure(
                    bTerminalFailure, FailureReason, "borrowed-release");
            LatchTerminalFailure(
                bTerminalFailure, FailureReason,
                RenderResult == ERHIResult::Failed
                    ? "render-fence-failed"
                    : RenderResult == ERHIResult::Timeout
                        ? "render-fence-timeout" : "render-fence");
            std::cerr << "borrowed-render-failure result="
                      << static_cast<int>(RenderResult)
                      << " release=" << static_cast<int>(ReleaseResult)
                      << '\n';
            return false;
        }
        FRHIRenderLease RenderLease;
        RenderLease.Frame = Target.Frame;
        RenderLease.FrameSlotIndex = FrameSlot;
        RenderLease.CompletionFence = RenderFence.Object;
        Present = Swapchain->PresentBorrowedTarget(
            Target, RenderLease, Lease);
    }
    else
    {
        Present = Swapchain->PresentBorrowedTarget(
            Target, Ready, Lease);
    }
    if (Present != ERHIResult::Success || !Lease.IsValid())
    {
        if (Present == ERHIResult::Success && !Lease.IsValid())
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-present-lease");
        else if (!IsRetryablePresentationResult(Present))
            LatchTerminalFailure(
                bTerminalFailure, FailureReason, "borrowed-present");
        // A failed pre-submit present may only release the drawable after the
        // deferred render fence proves that no caller work still references it.
        const ERHIResult RenderResult = WaitForRenderCompletion(
            RenderFence.Object, Window, std::chrono::seconds(5));
        if (RenderResult == ERHIResult::Success ||
            RenderResult == ERHIResult::Failed)
        {
            const ERHIResult ReleaseResult =
                Swapchain->ReleaseBorrowedTarget(Target, RenderFence.Object);
            if (ReleaseResult != ERHIResult::Success)
                LatchTerminalFailure(
                    bTerminalFailure, FailureReason, "borrowed-release");
            if (RenderResult == ERHIResult::Failed)
                LatchTerminalFailure(
                    bTerminalFailure, FailureReason, "render-fence-failed");
        }
        else
        {
            LatchTerminalFailure(
                bTerminalFailure, FailureReason,
                RenderResult == ERHIResult::Timeout
                    ? "render-fence-timeout" : "render-fence");
        }
        return false;
    }
    PresentationLeases.push_back(std::move(Lease));
    return RetireBorrowedLeases(
        PresentationLeases, bTerminalFailure, FailureReason);
}

bool TestLegacyAcquireWaitsForPendingBorrowed(
    const TSharedPtr<IRHISwapchain>& Swapchain,
    const TSharedPtr<IRHIPresentationSurface>& Surface,
    GLFWwindow* Window)
{
    using namespace Stoner::Backend::Metal::Private;
    if (!Swapchain || !Surface || !Window) return false;
    const auto NativeSurface =
        std::dynamic_pointer_cast<FMetalPresentationSurface>(Surface);
    const auto Context = NativeSurface ? NativeSurface->GetContext() : nullptr;
    if (!Context) return false;

    // AcquireBorrowedTarget always returns NotReady for a first request while
    // the worker owns nextDrawable.  Keep that unpublished request alive, then
    // verify that the legacy path cannot overwrite the same frame slot.
    constexpr uint64 FrameToken = 0x03003001;
    FRHIBorrowedAcquiredTarget PendingTarget;
    const ERHIResult StartResult = Swapchain->AcquireBorrowedTarget(
        FrameToken, 0, PendingTarget);
    const uint32 PendingBeforeLegacy =
        Context->GetPendingDrawableAcquireCount();
    if (StartResult != ERHIResult::NotReady || PendingBeforeLegacy != 1)
        return false;

    uint32 FormalFrame = 0;
    const ERHIResult LegacyResult =
        Swapchain->AcquireNextFrame(FormalFrame);
    const uint32 PendingAfterLegacy =
        Context->GetPendingDrawableAcquireCount();
    if (LegacyResult != ERHIResult::NotReady ||
        PendingAfterLegacy != PendingBeforeLegacy)
        return false;

    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    for (;;)
    {
        glfwPollEvents();
        FRHIBorrowedAcquiredTarget Target;
        const ERHIResult Retry = Swapchain->AcquireBorrowedTarget(
            FrameToken, 0, Target);
        if (Retry == ERHIResult::Success)
        {
            const bool bValid = Target.IsValid() && Target.Texture;
            const ERHIResult Release =
                Swapchain->ReleaseBorrowedTarget(Target, nullptr);
            return bValid && Release == ERHIResult::Success;
        }
        if (Retry != ERHIResult::NotReady &&
            Retry != ERHIResult::Unavailable &&
            Retry != ERHIResult::ResizeRequired)
            return false;
        if (std::chrono::steady_clock::now() >= Deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
#endif

} // namespace

int main(int ArgCount, char** Arguments)
{
    FOptions Options;
    if (!ParseOptions(ArgCount, Arguments, Options))
    {
        std::cerr << "usage: MetalPresentationProbe [--frames N] "
            "[--cycles N] [--report PATH] [--borrowed-preview]\n";
        return 2;
    }
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    WriteReport(Options, 0, 0, "unavailable", "glfw-unavailable");
    return 3;
#else
    FWindowDesc WindowDesc;
    WindowDesc.Title = "Stoner Metal Presentation Validation";
    WindowDesc.ClientWidth = 640;
    WindowDesc.ClientHeight = 360;
    WindowDesc.bVisible = true;
    FWindow Window;
    if (Window.CreateRealWindow(WindowDesc) != EApplicationResult::Success)
    {
        WriteReport(Options, 0, 0, "unavailable", "window-unavailable");
        return 3;
    }
    auto Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        (void)Window.Destroy();
        WriteReport(Options, 0, 0, "unavailable", "metal-device-unavailable");
        return 3;
    }
    FRHIPresentationSurfaceDesc SurfaceDesc;
    SurfaceDesc.Window = Window.GetPlatformWindow();
    auto Surface = Created.Device->CreatePresentationSurface(SurfaceDesc);
    FRHISwapchainDesc SwapchainDesc;
    SwapchainDesc.Width = Window.GetDrawableWidth();
    SwapchainDesc.Height = Window.GetDrawableHeight();
    SwapchainDesc.FramesInFlight = 2;
    FRHIPresentationCapabilities PresentationCapabilities;
    const ERHIResult CapabilityResult = Surface.Succeeded()
        ? Surface.Object->QueryCapabilities(PresentationCapabilities)
        : ERHIResult::InvalidState;
    SwapchainDesc.SurfaceCapabilityGeneration =
        PresentationCapabilities.CapabilityGeneration;
    auto Swapchain = CapabilityResult == ERHIResult::Success
        ? Created.Device->CreateSwapchain(Surface.Object, SwapchainDesc)
        : TRHIObjectResult<IRHISwapchain>{};
    auto Queue = Created.Device->CreateCommandQueue(ERHIQueueType::Graphics);
    if (!Surface.Succeeded() || !Swapchain.Succeeded() || !Queue.Succeeded())
    {
        if (Surface.Succeeded()) (void)Surface.Object->Invalidate();
        (void)Created.Device->Shutdown();
        (void)Window.Destroy();
        WriteReport(Options, 0, 0, "failed", "presentation-initialization");
        return 4;
    }
    if (Options.bBorrowedPreview &&
        !PresentationCapabilities.bSupportsIndependentPresentationCompletion)
    {
        (void)Surface.Object->Invalidate();
        (void)Created.Device->Shutdown();
        (void)Window.Destroy();
        WriteReport(Options, 0, 0, "unavailable",
            "independent-presentation-completion-unavailable");
        return 3;
    }

    auto* NativeWindow = static_cast<GLFWwindow*>(
        Window.GetPlatformWindow().GetNativeHandle());
    uint32 Presented = 0;
    uint32 Cycles = 0;
    TArray<FRHIPresentationLease> PresentationLeases;
    const uint32 CycleInterval =
        Options.Frames / (Options.LifecycleCycles + 1);
    const uint32 MaximumAttempts = Options.Frames * 100;
    const uint32 EffectiveCycleInterval = CycleInterval > 0
        ? CycleInterval : 1;
    bool bLifecycleFailed = false;
    bool bPendingOwnership = true;
    bool bTerminalFailure = false;
    const char* TerminalFailureReason = nullptr;
    if (Options.bBorrowedPreview)
    {
        bPendingOwnership = TestLegacyAcquireWaitsForPendingBorrowed(
            Swapchain.Object, Surface.Object, NativeWindow);
        if (!bPendingOwnership)
        {
            bLifecycleFailed = true;
            LatchTerminalFailure(
                bTerminalFailure, TerminalFailureReason,
                "legacy-acquire-pending-ownership");
        }
    }
    for (uint32 Attempt = 0;
         Presented < Options.Frames && Attempt < MaximumAttempts;
         ++Attempt)
    {
        (void)Window.PollEvents();
        if (Cycles < Options.LifecycleCycles &&
            Presented >= (Cycles + 1) * EffectiveCycleInterval)
        {
            if (Options.bBorrowedPreview &&
                !DrainBorrowedLeases(
                    PresentationLeases, NativeWindow,
                    std::chrono::seconds(5), bTerminalFailure,
                    TerminalFailureReason))
            {
                bLifecycleFailed = true;
                break;
            }
            const int Width = Cycles % 2 == 0 ? 800 : 640;
            const int Height = Cycles % 2 == 0 ? 450 : 360;
            glfwSetWindowSize(NativeWindow, Width, Height);
            glfwIconifyWindow(NativeWindow);
            if (!WaitForWindowAttribute(
                    NativeWindow, GLFW_ICONIFIED, GLFW_TRUE))
            {
                bLifecycleFailed = true;
                break;
            }
            glfwRestoreWindow(NativeWindow);
            if (!WaitForWindowAttribute(
                    NativeWindow, GLFW_ICONIFIED, GLFW_FALSE))
            {
                bLifecycleFailed = true;
                break;
            }
            if (!WaitForDrawableExtent(NativeWindow, Width, Height))
            {
                bLifecycleFailed = true;
                break;
            }
            ++Cycles;
        }
        const bool bFrame = Options.bBorrowedPreview
            ? RenderBorrowedFrame(
                Created.Device, Queue.Object, Swapchain.Object,
                Presented + 1, Presented % MaxRHIFrameSlots,
                PresentationLeases, NativeWindow, bTerminalFailure,
                TerminalFailureReason)
            : RenderClearFrame(Created.Device, Queue.Object, Swapchain.Object);
        if (bFrame)
            ++Presented;
        else if (bTerminalFailure)
            break;
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (Options.bBorrowedPreview)
    {
        (void)DrainBorrowedLeases(
            PresentationLeases, NativeWindow, std::chrono::seconds(5),
            bTerminalFailure, TerminalFailureReason);
        for (const auto& Lease : PresentationLeases)
        {
            const auto Poll = Lease.PresentationCompletionFence
                ? Lease.PresentationCompletionFence->Wait(0)
                : ERHIResult::InvalidState;
            std::cerr << "pending-presentation frame="
                << Lease.Frame.FrameToken << " poll="
                << static_cast<int>(Poll) << '\n';
        }
    }

    const bool bPresentedAll = !bLifecycleFailed && bPendingOwnership &&
        !bTerminalFailure &&
        Presented == Options.Frames && Cycles == Options.LifecycleCycles &&
        PresentationLeases.empty();
    const auto SurfaceShutdown = Surface.Object->Invalidate();
    const auto DeviceShutdown = Created.Device->Shutdown();
    const auto WindowShutdown = Window.Destroy();
    Swapchain.Object.reset();
    Surface.Object.reset();
    Queue.Object.reset();
    FMetalBackendInspection Inspection;
    const bool bOwnershipClean = InspectMetalDevice(
            Created.Device, Inspection) &&
        Inspection.LiveObjectCount == 0 &&
        Inspection.PresentationOwnershipCount == 0 &&
        Inspection.InFlightSubmissionCount == 0;
    const bool bClean = SurfaceShutdown == ERHIResult::Success &&
        DeviceShutdown == ERHIResult::Success &&
        WindowShutdown == EApplicationResult::Success && bOwnershipClean;
    const char* FailureReasonForReport =
        bTerminalFailure && TerminalFailureReason
            ? TerminalFailureReason
            : (bPresentedAll ? (bClean ? "" : "shutdown") : "frame-budget");
    WriteReport(Options, Presented, Cycles,
        bPresentedAll && bClean ? "passed" : "failed",
        FailureReasonForReport,
        SurfaceShutdown == ERHIResult::Success,
        DeviceShutdown == ERHIResult::Success,
        WindowShutdown == EApplicationResult::Success,
        bOwnershipClean, PresentationLeases.empty());
    std::cout << "metal-presentation frames=" << Presented
              << " cycles=" << Cycles
              << " shutdown=" << (bClean ? "clean" : "failed") << '\n';
    return bPresentedAll && bClean ? 0 : 5;
#endif
}
