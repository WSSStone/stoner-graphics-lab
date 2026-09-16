#include "MetalPresentationIntegrationTests.h"

#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "Core/SGPlatform.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "RHI/RHIMinimal.h"
#if SG_PLATFORM_MAC
#include "../Source/Backend/Metal/Private/FMetalPresentationSurface.h"
#endif

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    #define GLFW_INCLUDE_NONE
    #include <GLFW/glfw3.h>
#endif

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace
{

using namespace Stoner;
using namespace Stoner::Application;
using namespace Stoner::Backend::Metal;
using namespace Stoner::RHI;

void Record(FMetalPresentationIntegrationTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

#if SG_PLATFORM_MAC && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
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

ERHIResult AcquireAfterRestore(
    const Core::TSharedPtr<IRHISwapchain>& Swapchain,
    Core::uint32& OutFrame)
{
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    ERHIResult Result = ERHIResult::Unavailable;
    do
    {
        glfwPollEvents();
        Result = Swapchain->AcquireNextFrame(OutFrame);
        if (Result == ERHIResult::Success) return Result;
        if (Result != ERHIResult::Unavailable &&
            Result != ERHIResult::NotReady &&
            Result != ERHIResult::ResizeRequired)
            return Result;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (std::chrono::steady_clock::now() < Deadline);
    return Result;
}
#endif

#if SG_PLATFORM_MAC && defined(STONER_GLFW_AVAILABLE) && \
    STONER_GLFW_AVAILABLE
bool TestBorrowedAcquireCancellation(
    Core::TSharedPtr<RHI::IRHISwapchain>& Swapchain,
    const Core::TSharedPtr<RHI::IRHIPresentationSurface>& Surface)
{
    using namespace Stoner::Backend::Metal::Private;
    if (!Swapchain || !Surface) return false;
    const auto NativeSurface =
        std::dynamic_pointer_cast<FMetalPresentationSurface>(Surface);
    const auto Context = NativeSurface ? NativeSurface->GetContext() : nullptr;
    if (!Context) return false;
    const auto Fail = [&](const char* Stage) {
        std::cout << "[INFO] borrowed-cancel-regression"
                  << " stage=" << Stage
                  << " pending=" << Context->GetPendingDrawableAcquireCount()
                  << " leases=" << Context->GetPendingPresentationLeaseCount()
                  << '\n';
        return false;
    };

    // Fill both production frame slots with unpublished async acquisitions.
    // Slot zero may still be completing the formal presentation above, so
    // retry only until its pending record is admitted; do not poll it and
    // accidentally publish a drawable before the reset regression.
    const auto StartPending = [&](Core::uint32 FrameSlot,
                                  Core::uint64 FrameToken) {
        const auto Deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        do
        {
            RHI::FRHIBorrowedAcquiredTarget Target;
            const RHI::ERHIResult AcquireResult =
                Swapchain->AcquireBorrowedTarget(
                    FrameToken, FrameSlot, Target);
            const Core::uint32 PendingCount =
                Context->GetPendingDrawableAcquireCount();
            if (PendingCount > RHI::MaxRHIFrameSlots ||
                Context->GetPendingPresentationLeaseCount() >
                    RHI::MaxRHIPresentationImageLeases)
                return Fail("bound");
            if (PendingCount >= FrameSlot + 1)
                return AcquireResult == RHI::ERHIResult::NotReady;
            if (AcquireResult != RHI::ERHIResult::NotReady &&
                AcquireResult != RHI::ERHIResult::Unavailable &&
                AcquireResult != RHI::ERHIResult::ResizeRequired)
                return Fail("start-result");
            glfwPollEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        while (std::chrono::steady_clock::now() < Deadline);
        return false;
    };

    if (!StartPending(0, 4001)) return Fail("slot-zero");
    if (!StartPending(1, 4002)) return Fail("slot-one");
    const bool bTwoPending =
        Context->GetPendingDrawableAcquireCount() == 2 &&
        Context->GetPendingPresentationLeaseCount() == 2;
    if (!bTwoPending) return Fail("two-pending");

    // Destruction must cancel every frame record owned by the context.  The
    // surface remains alive, allowing each worker's completion callback to
    // retire its canceled record without a new render or surface shutdown.
    Swapchain.reset();
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(5);
    do
    {
        if (Context->GetPendingDrawableAcquireCount() == 0 &&
            Context->GetPendingPresentationLeaseCount() == 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    while (std::chrono::steady_clock::now() < Deadline);
    return Fail("retire-timeout");
}
#endif

} // namespace

FMetalPresentationIntegrationTestResult
RunMetalPresentationIntegrationTests(bool bRequireVisible)
{
    FMetalPresentationIntegrationTestResult Result;
    if (!bRequireVisible)
    {
        Record(Result, true,
            "visible Metal presentation is opt-in and was not requested");
        return Result;
    }
#if SG_PLATFORM_MAC
    FWindowDesc WindowDesc;
    WindowDesc.Title = "Stoner Metal Presentation Probe";
    WindowDesc.ClientWidth = 640;
    WindowDesc.ClientHeight = 360;
    WindowDesc.bVisible = true;
    FWindow Window;
    const auto WindowResult = Window.CreateRealWindow(WindowDesc);
    const auto Created = CreateMetalDevice();
    if (WindowResult != EApplicationResult::Success || !Created.Succeeded())
    {
        if (Created.Succeeded()) (void)Created.Device->Shutdown();
        if (Window.IsRealWindow()) (void)Window.Destroy();
        Record(Result, false,
            "required visible Metal window and native device are available");
        return Result;
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
    Record(Result,
        CapabilityResult == ERHIResult::Success &&
            PresentationCapabilities.bSupportsIndependentPresentationCompletion &&
            PresentationCapabilities.PresentationRetirementMode ==
                ERHIPresentationRetirementMode::NativeCallback,
        "Metal native surface identifies callback presentation retirement");
    SwapchainDesc.SurfaceCapabilityGeneration =
        PresentationCapabilities.CapabilityGeneration;
    auto Swapchain = CapabilityResult == ERHIResult::Success
        ? Created.Device->CreateSwapchain(Surface.Object, SwapchainDesc)
        : TRHIObjectResult<IRHISwapchain>{};
    Core::uint32 FrameIndex = 0;
    const ERHIResult InitialAcquire = Swapchain.Succeeded()
        ? Swapchain.Object->AcquireNextFrame(FrameIndex)
        : ERHIResult::InvalidState;
    const bool Acquired = Swapchain.Succeeded() &&
        InitialAcquire == ERHIResult::Success &&
        Swapchain.Object->GetImage(FrameIndex) != nullptr;
    const ERHIResult InitialPresent = Acquired
        ? Swapchain.Object->Present(FrameIndex)
        : ERHIResult::InvalidState;
    const bool Presented = Acquired && InitialPresent == ERHIResult::Success;

    bool bLifecycle = false;
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    bool bBorrowedCancellation = false;
    auto* NativeWindow = static_cast<GLFWwindow*>(
        Window.GetPlatformWindow().GetNativeHandle());
    glfwSetWindowSize(NativeWindow, 800, 450);
    (void)Window.PollEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    (void)Window.PollEvents();
    Core::uint32 ResizedFrame = 0;
    const ERHIResult ResizeAcquire = Presented
        ? AcquireAfterRestore(Swapchain.Object, ResizedFrame)
        : ERHIResult::InvalidState;
    const bool bResized = Presented && ResizeAcquire == ERHIResult::Success &&
        Swapchain.Object->GetImage(ResizedFrame) != nullptr &&
        Swapchain.Object->GetImage(ResizedFrame)->GetDesc().Width > 0 &&
        Swapchain.Object->Present(ResizedFrame) == ERHIResult::Success;

    glfwIconifyWindow(NativeWindow);
    const bool bIconified = WaitForWindowAttribute(
        NativeWindow, GLFW_ICONIFIED, GLFW_TRUE);
    Core::uint32 PausedFrame = 0;
    const ERHIResult PauseAcquire =
        Swapchain.Object->AcquireNextFrame(PausedFrame);
    const bool bPaused = bIconified &&
        PauseAcquire == ERHIResult::Unavailable;

    glfwRestoreWindow(NativeWindow);
    const bool bUniconified = WaitForWindowAttribute(
        NativeWindow, GLFW_ICONIFIED, GLFW_FALSE);
    Core::uint32 RestoredFrame = 0;
    const ERHIResult RestoreAcquire = bUniconified
        ? AcquireAfterRestore(Swapchain.Object, RestoredFrame)
        : ERHIResult::Unavailable;
    const bool bRestored = bUniconified &&
        RestoreAcquire == ERHIResult::Success &&
        Swapchain.Object->Present(RestoredFrame) == ERHIResult::Success;
    (void)Window.RequestClose();
    Core::uint32 ClosingFrame = 0;
    const bool bCloseRejected =
        Swapchain.Object->AcquireNextFrame(ClosingFrame) ==
            ERHIResult::Unavailable;

    // The close rejection above intentionally leaves the native window's
    // should-close bit set.  Restore that bit for the focused destruction
    // regression; FWindow remains close-requested and is destroyed below.
    glfwSetWindowShouldClose(NativeWindow, GLFW_FALSE);
#if SG_PLATFORM_MAC && defined(STONER_GLFW_AVAILABLE) && \
    STONER_GLFW_AVAILABLE
    if (Acquired && Presented && Swapchain.Succeeded() && Surface.Succeeded())
        bBorrowedCancellation = TestBorrowedAcquireCancellation(
            Swapchain.Object, Surface.Object);
#else
    bBorrowedCancellation = true;
#endif
    bLifecycle = bResized && bPaused && bRestored && bCloseRejected &&
        bBorrowedCancellation;
    if (!bLifecycle)
    {
        std::cout << "[INFO] visible-metal-lifecycle"
                  << " initial-acquire=" << static_cast<int>(InitialAcquire)
                  << " initial-present=" << static_cast<int>(InitialPresent)
                  << " resize-acquire=" << static_cast<int>(ResizeAcquire)
                  << " iconified=" << bIconified
                  << " paused-acquire=" << static_cast<int>(PauseAcquire)
                  << " uniconified=" << bUniconified
                  << " restore-acquire=" << static_cast<int>(RestoreAcquire)
                  << " close-rejected=" << bCloseRejected
                  << " borrowed-cancel=" << bBorrowedCancellation << '\n';
    }
#endif
    if (Surface.Succeeded()) (void)Surface.Object->Invalidate();
    const auto Shutdown = Created.Device->Shutdown();
    const auto Destroyed = Window.Destroy();
    Record(Result,
        Acquired && Presented && bLifecycle &&
            Shutdown == ERHIResult::Success &&
            Destroyed == EApplicationResult::Success,
        "visible Metal resize minimize restore close and detach lifecycle passes");
#else
    Record(Result, false,
        "required visible Metal presentation is unavailable off macOS");
#endif
    return Result;
}
