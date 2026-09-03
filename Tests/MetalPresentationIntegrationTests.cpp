#include "MetalPresentationIntegrationTests.h"

#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "Core/SGPlatform.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "RHI/RHIMinimal.h"

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    #define GLFW_INCLUDE_NONE
    #include <GLFW/glfw3.h>
#endif

#include <chrono>
#include <iostream>
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
    auto* NativeWindow = static_cast<GLFWwindow*>(
        Window.GetPlatformWindow().GetNativeHandle());
    glfwSetWindowSize(NativeWindow, 800, 450);
    (void)Window.PollEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    (void)Window.PollEvents();
    Core::uint32 ResizedFrame = 0;
    const ERHIResult ResizeAcquire = Presented
        ? Swapchain.Object->AcquireNextFrame(ResizedFrame)
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
    bLifecycle = bResized && bPaused && bRestored && bCloseRejected;
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
                  << " close-rejected=" << bCloseRejected << '\n';
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
