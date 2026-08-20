#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "RHI/RHIMinimal.h"

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
            else if (!ParseCount(Arguments[Index],
                Argument == "--frames" ? Out.Frames : Out.LifecycleCycles))
                return false;
        }
        else return false;
    }
    return Out.Frames >= Out.LifecycleCycles;
}

void WriteReport(const FOptions& Options, uint32 Presented, uint32 Cycles,
    const char* Result, const char* Failure, bool bLayerDetached = false,
    bool bDeviceShutdown = false, bool bWindowDestroyed = false,
    bool bOwnershipClean = false)
{
    if (Options.ReportPath.IsEmpty()) return;
    std::ofstream Output(std::string(Options.ReportPath.View()),
        std::ios::binary | std::ios::trunc);
    if (!Output) return;
    Output << "{\n"
        << "  \"schema\": \"stoner.metal.presentation-probe.v1\",\n"
        << "  \"requestedFrames\": " << Options.Frames << ",\n"
        << "  \"presentedFrames\": " << Presented << ",\n"
        << "  \"requestedLifecycleCycles\": "
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
#endif

} // namespace

int main(int ArgCount, char** Arguments)
{
    FOptions Options;
    if (!ParseOptions(ArgCount, Arguments, Options))
    {
        std::cerr << "usage: MetalPresentationProbe [--frames N] "
            "[--cycles N] [--report PATH]\n";
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
    auto Swapchain = Surface.Succeeded()
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

    auto* NativeWindow = static_cast<GLFWwindow*>(
        Window.GetPlatformWindow().GetNativeHandle());
    uint32 Presented = 0;
    uint32 Cycles = 0;
    const uint32 CycleInterval =
        Options.Frames / (Options.LifecycleCycles + 1);
    const uint32 MaximumAttempts = Options.Frames * 100;
    const uint32 EffectiveCycleInterval = CycleInterval > 0
        ? CycleInterval : 1;
    bool bLifecycleFailed = false;
    for (uint32 Attempt = 0;
         Presented < Options.Frames && Attempt < MaximumAttempts;
         ++Attempt)
    {
        (void)Window.PollEvents();
        if (Cycles < Options.LifecycleCycles &&
            Presented >= (Cycles + 1) * EffectiveCycleInterval)
        {
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
            ++Cycles;
        }
        if (RenderClearFrame(Created.Device, Queue.Object, Swapchain.Object))
            ++Presented;
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const bool bPresentedAll = !bLifecycleFailed &&
        Presented == Options.Frames && Cycles == Options.LifecycleCycles;
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
    WriteReport(Options, Presented, Cycles,
        bPresentedAll && bClean ? "passed" : "failed",
        bPresentedAll ? (bClean ? "" : "shutdown") : "frame-budget",
        SurfaceShutdown == ERHIResult::Success,
        DeviceShutdown == ERHIResult::Success,
        WindowShutdown == EApplicationResult::Success,
        bOwnershipClean);
    std::cout << "metal-presentation frames=" << Presented
              << " cycles=" << Cycles
              << " shutdown=" << (bClean ? "clean" : "failed") << '\n';
    return bPresentedAll && bClean ? 0 : 5;
#endif
}
