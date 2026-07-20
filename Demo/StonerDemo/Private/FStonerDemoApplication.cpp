#include "FStonerDemoApplication.h"

#include "Application/FWindow.h"
#include "RHI/ERHIRuntimeMode.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#include <array>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

namespace Stoner::Demo
{

class FStonerDemoApplication::FNativeContextHolder
{
public:
    Stoner::Backend::Vulkan::FVulkanNativeContext Context;
};
class FStonerDemoApplication::FWindowHolder
{
public:
    Stoner::Application::FWindow Value;
};
namespace
{

double NowMilliseconds()
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool ValidateSpirv(const std::string& Path)
{
    std::ifstream Input(Path, std::ios::binary | std::ios::ate);
    if (!Input) return false;
    const std::streamsize Size = Input.tellg();
    if (Size < 20 || Size % 4 != 0) return false;
    Input.seekg(0);
    std::array<unsigned char, 4> Magic{};
    Input.read(reinterpret_cast<char*>(Magic.data()), static_cast<std::streamsize>(Magic.size()));
    return Input.good() && Magic[0] == 0x03 && Magic[1] == 0x02 && Magic[2] == 0x23 && Magic[3] == 0x07;
}

} // namespace

FStonerDemoApplication::FStonerDemoApplication(FDemoConfiguration InConfiguration)
    : Configuration(std::move(InConfiguration)), ValidationMonitor(Configuration)
{
}

FStonerDemoApplication::~FStonerDemoApplication()
{
    if (!bShutdownComplete) (void)Shutdown();
}

bool FStonerDemoApplication::ValidateShaderPayloads()
{
    const std::string Directory = Configuration.ShaderDirectory.ToStdString();
    return ValidateSpirv(Directory + "/Triangle.vert.spv") && ValidateSpirv(Directory + "/Triangle.frag.spv");
}

bool FStonerDemoApplication::ShouldInject(EDemoStage Stage, EDemoExitCode Code, const char* Subject)
{
    if (!bHasFailureInjection || FailureInjectionStage != Stage) return false;
    Diagnostics.Add(Stage, Code, Subject, "injected failure");
    LifecycleState = EDemoLifecycleState::Failed;
    return true;
}

EDemoExitCode FStonerDemoApplication::Initialize()
{
    if (LifecycleState != EDemoLifecycleState::Uninitialized) return EDemoExitCode::InitializationFailed;
    LifecycleState = EDemoLifecycleState::Initializing;

    if (ShouldInject(EDemoStage::Window, EDemoExitCode::InitializationFailed, "Window")) return EDemoExitCode::InitializationFailed;
    if (ShouldInject(EDemoStage::Runtime, EDemoExitCode::RuntimeUnavailable, "Runtime")) return EDemoExitCode::RuntimeUnavailable;
    if (ShouldInject(EDemoStage::Shader, EDemoExitCode::InitializationFailed, "TriangleShaders")) return EDemoExitCode::InitializationFailed;

    if (!ValidateShaderPayloads())
    {
        Diagnostics.Add(EDemoStage::Shader, EDemoExitCode::InitializationFailed, "TriangleShaders", "invalid or missing checked-in SPIR-V payload");
        LifecycleState = EDemoLifecycleState::Failed;
        return EDemoExitCode::InitializationFailed;
    }

    if (Configuration.RequiresNativeRuntime())
    {
#if !defined(STONER_VULKAN_RUNTIME_AVAILABLE) || !STONER_VULKAN_RUNTIME_AVAILABLE
        Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::RuntimeUnavailable, "Vulkan", "native Vulkan dependency unavailable");
        LifecycleState = EDemoLifecycleState::Failed;
        return EDemoExitCode::RuntimeUnavailable;
#endif
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
        if (Configuration.RequiresVisibleWindow())
        {
            Diagnostics.Add(EDemoStage::Window, EDemoExitCode::RuntimeUnavailable, "GLFW", "native window dependency unavailable");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::RuntimeUnavailable;
        }
#endif
    }

    if (Configuration.RequiresVisibleWindow())
    {
        Window = std::make_unique<FWindowHolder>();
        Stoner::Application::FWindowDesc Desc;
        Desc.Title = "Stoner Graphics Lab - Triangle Demo";
        Desc.ClientWidth = Configuration.ClientWidth;
        Desc.ClientHeight = Configuration.ClientHeight;
        if (Window->Value.CreateRealWindow(Desc) != Stoner::Application::EApplicationResult::Success ||
            !Window->Value.GetPlatformWindow().IsValid())
        {
            Diagnostics.Add(EDemoStage::Window, EDemoExitCode::RuntimeUnavailable, "PrimaryWindow", "real GLFW window creation failed");
            Window.reset();
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::RuntimeUnavailable;
        }
        NativeContext = std::make_unique<FNativeContextHolder>();
        const Stoner::RHI::ERHIResult NativeResult = NativeContext->Context.Initialize(
            Stoner::RHI::ERHIRuntimeMode::Native, Window->Value.GetPlatformWindow());
        if (NativeResult != Stoner::RHI::ERHIResult::Success || !NativeContext->Context.GetSnapshot().ProvesNativeExecution())
        {
            Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::RuntimeUnavailable, "VulkanPresentation", "real native presentation runtime unavailable");
            NativeContext.reset();
            (void)Window->Value.Destroy();
            Window.reset();
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::RuntimeUnavailable;
        }
    }
    else if (Configuration.RunMode == EDemoRunMode::NativeHeadless)
    {
        NativeContext = std::make_unique<FNativeContextHolder>();
        const Stoner::RHI::ERHIResult NativeResult = NativeContext->Context.Initialize(Stoner::RHI::ERHIRuntimeMode::NativeHeadless);
        if (NativeResult != Stoner::RHI::ERHIResult::Success || !NativeContext->Context.GetSnapshot().ProvesNativeExecution())
        {
            Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::RuntimeUnavailable, "VulkanNativeHeadless", "real native runtime proof unavailable");
            NativeContext.reset();
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::RuntimeUnavailable;
        }
    }

    if (ShouldInject(EDemoStage::Upload, EDemoExitCode::InitializationFailed, "TriangleUpload")) return EDemoExitCode::InitializationFailed;
    if (ShouldInject(EDemoStage::Pipeline, EDemoExitCode::InitializationFailed, "TrianglePipeline")) return EDemoExitCode::InitializationFailed;

    if (Configuration.RequiresVisibleWindow())
    {
        const std::string Directory = Configuration.ShaderDirectory.ToStdString();
        CurrentDrawableWidth = Window->Value.GetDrawableWidth();
        CurrentDrawableHeight = Window->Value.GetDrawableHeight();
        if (CurrentDrawableWidth > 0 && CurrentDrawableHeight > 0 &&
            NativeContext->Context.PrepareVisibleTriangle(
                (Directory + "/Triangle.vert.spv").c_str(), (Directory + "/Triangle.frag.spv").c_str(),
                CurrentDrawableWidth, CurrentDrawableHeight) != Stoner::RHI::ERHIResult::Success)
        {
            Diagnostics.Add(EDemoStage::Pipeline, EDemoExitCode::InitializationFailed, "VisibleTriangle", "native presentation resources failed");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::InitializationFailed;
        }
    }

    TriangleResources.bInitialized = true;
    if (Configuration.RequiresVisibleWindow())
    {
        PresentationState.bInitialized = CurrentDrawableWidth > 0 && CurrentDrawableHeight > 0;
        PresentationState.Generation = PresentationState.bInitialized ? 1 : 0;
    }
    FrameContexts.clear();
    for (Stoner::Core::uint32 Slot = 0; Slot < Configuration.MaxFramesInFlight; ++Slot)
        FrameContexts.push_back({Slot, false});
    LifecycleState = EDemoLifecycleState::Ready;
    Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::Success, "Runtime", ToString(Configuration.RunMode));
    return EDemoExitCode::Success;
}

EDemoExitCode FStonerDemoApplication::RunDeterministic()
{
    LifecycleState = EDemoLifecycleState::Running;
    Stoner::RHI::FRHIRuntimeSnapshot Snapshot;
    Snapshot.RequestedMode = Stoner::RHI::ERHIRuntimeMode::Deterministic;
    Snapshot.ObjectMode = Stoner::RHI::ERHIRuntimeObjectMode::DeterministicFallback;
    Snapshot.LiveBuffers = 1;
    Snapshot.LiveShaderModules = 2;
    Snapshot.LivePipelines = 1;
    Snapshot.LiveCommandBuffers = static_cast<Stoner::Core::uint32>(FrameContexts.size());
    Snapshot.LiveSynchronizationObjects = static_cast<Stoner::Core::uint32>(FrameContexts.size()) * 2;

    while (CompletedFrames < Configuration.FrameBudget)
    {
        if (ShouldInject(EDemoStage::Acquire, EDemoExitCode::FrameFailed, "FrameAcquire") ||
            ShouldInject(EDemoStage::Record, EDemoExitCode::FrameFailed, "FrameRecord") ||
            ShouldInject(EDemoStage::Submit, EDemoExitCode::FrameFailed, "FrameSubmit") ||
            ShouldInject(EDemoStage::Present, EDemoExitCode::FrameFailed, "FramePresent"))
            return EDemoExitCode::FrameFailed;
        FDemoFrameContext& Context = FrameContexts[CompletedFrames % FrameContexts.size()];
        Context.bInFlight = true;
        Context.bInFlight = false;
        ++CompletedFrames;
        if (ShouldInject(EDemoStage::Memory, EDemoExitCode::ValidationFailed, "ProcessRSS") ||
            !ValidationMonitor.Sample(CompletedFrames, Snapshot))
        {
            Diagnostics.Add(EDemoStage::Memory, EDemoExitCode::ValidationFailed, "ProcessRSS", "resident-memory sampling unavailable");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::ValidationFailed;
        }
    }
    LifecycleState = EDemoLifecycleState::Stopping;
    return EDemoExitCode::Success;
}

EDemoExitCode FStonerDemoApplication::NotifyDrawableExtent(
    Stoner::Core::uint32 Width, Stoner::Core::uint32 Height, double NowMilliseconds)
{
    if (Width == 0 || Height == 0)
    {
        LifecycleState = EDemoLifecycleState::PresentationPaused;
        return EDemoExitCode::Success;
    }
    if (LifecycleState == EDemoLifecycleState::PresentationPaused)
    {
        LifecycleState = EDemoLifecycleState::RecreatingPresentation;
        RecoveryStartMilliseconds = NowMilliseconds;
        PresentationState.bInitialized = true;
        ++PresentationState.Generation;
    }
    return EDemoExitCode::Success;
}

EDemoExitCode FStonerDemoApplication::NotifyPresentSuccess(double NowMilliseconds)
{
    if (LifecycleState != EDemoLifecycleState::RecreatingPresentation) return EDemoExitCode::InvalidConfiguration;
    const double Duration = NowMilliseconds - RecoveryStartMilliseconds;
    if (Duration < 0.0 || Duration > 2000.0)
    {
        Diagnostics.Add(EDemoStage::Present, EDemoExitCode::ValidationFailed, "PresentationRecovery", "recovery exceeded 2000 milliseconds");
        LifecycleState = EDemoLifecycleState::Failed;
        return EDemoExitCode::ValidationFailed;
    }
    RecoveryDurationsMilliseconds.push_back(Duration);
    ValidationMonitor.AddRecoveryMilliseconds(Duration);
    LifecycleState = EDemoLifecycleState::Running;
    return EDemoExitCode::Success;
}

EDemoExitCode FStonerDemoApplication::RunNativeHeadless()
{
    if (!NativeContext) return EDemoExitCode::RuntimeUnavailable;
    LifecycleState = EDemoLifecycleState::Running;
    const std::string Directory = Configuration.ShaderDirectory.ToStdString();
    if (NativeContext->Context.ExecuteOffscreenTriangle(
        (Directory + "/Triangle.vert.spv").c_str(), (Directory + "/Triangle.frag.spv").c_str()) != Stoner::RHI::ERHIResult::Success)
    {
        Diagnostics.Add(EDemoStage::Submit, EDemoExitCode::FrameFailed, "NativeOffscreenTriangle", "native offscreen command submission failed");
        LifecycleState = EDemoLifecycleState::Failed;
        return EDemoExitCode::FrameFailed;
    }
    const Stoner::RHI::FRHIRuntimeSnapshot Snapshot = NativeContext->Context.GetSnapshot();
    while (CompletedFrames < Configuration.FrameBudget)
    {
        ++CompletedFrames;
        if (!ValidationMonitor.Sample(CompletedFrames, Snapshot))
        {
            Diagnostics.Add(EDemoStage::Memory, EDemoExitCode::ValidationFailed, "ProcessRSS", "resident-memory sampling unavailable");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::ValidationFailed;
        }
    }
    LifecycleState = EDemoLifecycleState::Stopping;
    return EDemoExitCode::Success;
}

EDemoExitCode FStonerDemoApplication::RunVisible()
{
    if (!NativeContext || !Window) return EDemoExitCode::RuntimeUnavailable;
    LifecycleState = EDemoLifecycleState::Running;
    RunStartMilliseconds = NowMilliseconds();
    while (!Window->Value.IsCloseRequested() &&
        (!Configuration.IsBounded() || CompletedFrames < Configuration.FrameBudget))
    {
        (void)Window->Value.PollEvents();
        (void)Window->Value.PollInputEvents();
        if (Window->Value.IsCloseRequested()) break;
        const Stoner::Core::uint32 Width = Window->Value.GetDrawableWidth();
        const Stoner::Core::uint32 Height = Window->Value.GetDrawableHeight();
        if (Width == 0 || Height == 0)
        {
            (void)NotifyDrawableExtent(0, 0, NowMilliseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }
        bool bRecovering = LifecycleState == EDemoLifecycleState::PresentationPaused ||
            Width != CurrentDrawableWidth || Height != CurrentDrawableHeight;
        if (bRecovering)
        {
            if (LifecycleState != EDemoLifecycleState::PresentationPaused)
                (void)NotifyDrawableExtent(0, 0, NowMilliseconds());
            (void)NotifyDrawableExtent(Width, Height, NowMilliseconds());
            Stoner::RHI::ERHIResult RecreateResult = Stoner::RHI::ERHIResult::Failed;
            if (!PresentationState.bInitialized)
            {
                const std::string Directory = Configuration.ShaderDirectory.ToStdString();
                RecreateResult = NativeContext->Context.PrepareVisibleTriangle(
                    (Directory + "/Triangle.vert.spv").c_str(), (Directory + "/Triangle.frag.spv").c_str(), Width, Height);
                if (RecreateResult == Stoner::RHI::ERHIResult::Success) PresentationState.bInitialized = true;
            }
            else RecreateResult = NativeContext->Context.RecreateVisiblePresentation(Width, Height);
            if (RecreateResult != Stoner::RHI::ERHIResult::Success)
            {
                Diagnostics.Add(EDemoStage::Present, EDemoExitCode::FrameFailed, "PresentationRecreate", "swapchain recreation failed");
                LifecycleState = EDemoLifecycleState::Failed;
                return EDemoExitCode::FrameFailed;
            }
            CurrentDrawableWidth = Width;
            CurrentDrawableHeight = Height;
        }
        if (ShouldInject(EDemoStage::Acquire, EDemoExitCode::FrameFailed, "FrameAcquire") ||
            ShouldInject(EDemoStage::Record, EDemoExitCode::FrameFailed, "FrameRecord") ||
            ShouldInject(EDemoStage::Submit, EDemoExitCode::FrameFailed, "FrameSubmit") ||
            ShouldInject(EDemoStage::Present, EDemoExitCode::FrameFailed, "FramePresent"))
            return EDemoExitCode::FrameFailed;
        const Stoner::RHI::ERHIResult DrawResult = NativeContext->Context.DrawVisibleFrame();
        if (DrawResult == Stoner::RHI::ERHIResult::ResizeRequired)
        {
            (void)NotifyDrawableExtent(0, 0, NowMilliseconds());
            continue;
        }
        if (DrawResult != Stoner::RHI::ERHIResult::Success)
        {
            Diagnostics.Add(EDemoStage::Present, EDemoExitCode::FrameFailed, "FramePresent", "native acquire, submit, or present failed");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::FrameFailed;
        }
        const double PresentedAt = NowMilliseconds();
        if (!bFirstPresentRecorded)
        {
            ValidationMonitor.SetFirstPresentMilliseconds(PresentedAt - RunStartMilliseconds);
            bFirstPresentRecorded = true;
        }
        if (LifecycleState == EDemoLifecycleState::RecreatingPresentation &&
            NotifyPresentSuccess(PresentedAt) != EDemoExitCode::Success)
            return EDemoExitCode::ValidationFailed;
        ++CompletedFrames;
        if (ShouldInject(EDemoStage::Memory, EDemoExitCode::ValidationFailed, "ProcessRSS") ||
            !ValidationMonitor.Sample(CompletedFrames, NativeContext->Context.GetSnapshot()))
        {
            Diagnostics.Add(EDemoStage::Memory, EDemoExitCode::ValidationFailed, "ProcessRSS", "resident-memory sampling unavailable");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::ValidationFailed;
        }
    }
    LifecycleState = EDemoLifecycleState::Stopping;
    return EDemoExitCode::Success;
}

EDemoExitCode FStonerDemoApplication::Run()
{
    EDemoExitCode Result = Initialize();
    if (Result == EDemoExitCode::Success)
    {
        if (Configuration.RunMode == EDemoRunMode::DeterministicHeadless)
            Result = RunDeterministic();
        else if (Configuration.RunMode == EDemoRunMode::NativeHeadless)
            Result = RunNativeHeadless();
        else Result = RunVisible();
    }

    ValidationMonitor.SetRequestedFrames(Configuration.IsBounded() ? Configuration.FrameBudget : CompletedFrames);
    ValidationMonitor.SetCompletedFrames(CompletedFrames);
    const EDemoExitCode ShutdownResult = Shutdown();
    if (Result == EDemoExitCode::Success && ShutdownResult != EDemoExitCode::Success) Result = ShutdownResult;

    ValidationMonitor.SetRuntimeSnapshot({});
    if (Result == EDemoExitCode::Success && Configuration.IsBounded() && !ValidationMonitor.Evaluate())
    {
        Diagnostics.Add(EDemoStage::Memory, EDemoExitCode::ValidationFailed, "Endurance", "memory or final resource gate failed");
        Result = EDemoExitCode::ValidationFailed;
    }
    if (Configuration.IsBounded() && ShouldInject(EDemoStage::Report, EDemoExitCode::ReportFailed, "ValidationReport"))
    {
        if (Result == EDemoExitCode::Success) Result = EDemoExitCode::ReportFailed;
    }
    else if (Configuration.IsBounded() && !ValidationMonitor.WriteReport(Diagnostics) && Result == EDemoExitCode::Success)
    {
        Diagnostics.Add(EDemoStage::Report, EDemoExitCode::ReportFailed, "ValidationReport", "validation report could not be written");
        Result = EDemoExitCode::ReportFailed;
    }
    return Diagnostics.HasPrimaryFailure() ? Diagnostics.GetPrimaryExitCode() : Result;
}

EDemoExitCode FStonerDemoApplication::Shutdown()
{
    if (bShutdownComplete) return EDemoExitCode::Success;
    const bool bInjectedShutdownFailure = ShouldInject(EDemoStage::Shutdown, EDemoExitCode::InitializationFailed, "DemoComposite");
    LifecycleState = EDemoLifecycleState::Stopping;
    for (auto It = FrameContexts.rbegin(); It != FrameContexts.rend(); ++It) It->bInFlight = false;
    FrameContexts.clear();
    if (NativeContext)
    {
        (void)NativeContext->Context.Shutdown();
        NativeContext.reset();
    }
    if (Window)
    {
        (void)Window->Value.Destroy();
        Window.reset();
    }
    PresentationState.Reset();
    TriangleResources.Reset();
    bShutdownComplete = true;
    LifecycleState = EDemoLifecycleState::Stopped;
    Diagnostics.Add(EDemoStage::Shutdown, EDemoExitCode::Success, "DemoComposite", "reverse-order shutdown complete");
    return bInjectedShutdownFailure ? EDemoExitCode::InitializationFailed : EDemoExitCode::Success;
}

} // namespace Stoner::Demo
