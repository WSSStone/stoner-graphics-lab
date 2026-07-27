#include "FStonerDemoApplication.h"

#include "Application/FWindow.h"
#include "RHI/ERHIRuntimeMode.h"
#include "Renderer/RendererMinimal.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
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

bool ValidateSpirv(const std::string& Path, Stoner::Core::uint32 ExpectedExecutionModel)
{
    std::ifstream Input(Path, std::ios::binary | std::ios::ate);
    if (!Input) return false;
    const std::streamsize Size = Input.tellg();
    if (Size < 20 || Size % 4 != 0) return false;
    Input.seekg(0);
    Stoner::Core::TArray<Stoner::Core::uint32> Words(static_cast<std::size_t>(Size) / sizeof(Stoner::Core::uint32));
    Input.read(reinterpret_cast<char*>(Words.data()), Size);
    if (!Input.good() || Words[0] != 0x07230203u) return false;
    for (std::size_t Index = 5; Index < Words.size();)
    {
        const Stoner::Core::uint32 Instruction = Words[Index];
        const Stoner::Core::uint32 WordCount = Instruction >> 16u;
        const Stoner::Core::uint32 Opcode = Instruction & 0xffffu;
        if (WordCount == 0 || Index + WordCount > Words.size()) return false;
        if (Opcode == 15u && WordCount >= 4 && Words[Index + 1] == ExpectedExecutionModel)
        {
            const char* Name = reinterpret_cast<const char*>(&Words[Index + 3]);
            const std::size_t NameBytes = static_cast<std::size_t>(WordCount - 3) * sizeof(Stoner::Core::uint32);
            const void* Terminator = std::memchr(Name, '\0', NameBytes);
            if (Terminator != nullptr && std::string_view(Name, static_cast<const char*>(Terminator) - Name) == "main") return true;
        }
        Index += WordCount;
    }
    return false;
}

Stoner::Renderer::FForwardFramePlan BuildTriangleFramePlan(
    Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
{
    using namespace Stoner::Renderer;
    FForwardFrameInputs Inputs;
    Inputs.FrameName = "TriangleDemoFrame";
    Inputs.View.ViewName = "TriangleDemoView";
    Inputs.View.Viewport.Extent = {Width, Height};
    Inputs.Output.ColorTargetName = "PresentationColor";
    Inputs.Output.FormatSummary = "BGRA8";
    Inputs.Output.Extent = {Width, Height};
    Inputs.Environment.Mode = EForwardBackgroundMode::Clear;

    FMeshDrawCandidate Triangle;
    Triangle.ObjectId = 1;
    Triangle.MeshId = 1;
    Triangle.DebugName = "RGBTriangle";
    Triangle.bWantsOpaque = true;
    Triangle.MaterialBinding.MaterialId = 1;
    Triangle.MaterialBinding.MaterialName = "VertexColor";
    Triangle.MaterialBinding.bHasMaterialBinding = true;
    Triangle.MaterialBinding.bHasShaderBinding = true;
    Triangle.MaterialBinding.SurfaceInputs.bHasBaseColor = true;
    Triangle.MaterialBinding.SurfaceInputs.bHasMetallic = true;
    Triangle.MaterialBinding.SurfaceInputs.bHasRoughness = true;
    Triangle.MaterialBinding.SurfaceInputs.bHasNormal = true;
    Triangle.MaterialBinding.SurfaceInputs.bHasOcclusion = true;
    Triangle.MaterialBinding.SurfaceInputs.bHasEmissive = true;
    Triangle.MaterialBinding.SurfaceInputs.bHasAlpha = true;
    Inputs.DrawCandidates.push_back(std::move(Triangle));

    FForwardFramePlan Plan;
    FForwardRenderer Renderer;
    (void)Renderer.PrepareFrame(Inputs, Plan);
    return Plan;
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
    return ValidateSpirv(Directory + "/Triangle.vert.spv", 0u) &&
        ValidateSpirv(Directory + "/Triangle.frag.spv", 4u);
}

bool FStonerDemoApplication::ShouldInject(EDemoStage Stage, EDemoExitCode Code, const char* Subject)
{
    if (!bHasFailureInjection || FailureInjectionStage != Stage) return false;
    Diagnostics.Add(Stage, Code, Subject, "injected failure");
    LifecycleState = EDemoLifecycleState::Failed;
    return true;
}

EDemoExitCode FStonerDemoApplication::FailInitialize(
    EDemoStage Stage,
    EDemoExitCode Code,
    const char* Subject,
    const char* Reason)
{
    Diagnostics.Add(Stage, Code, Subject, Reason);
    LifecycleState = EDemoLifecycleState::Failed;
    (void)Shutdown();
    return Code;
}

EDemoExitCode FStonerDemoApplication::Initialize()
{
    if (LifecycleState != EDemoLifecycleState::Uninitialized) return EDemoExitCode::InitializationFailed;
    LifecycleState = EDemoLifecycleState::Initializing;

    if (ShouldInject(EDemoStage::Window, EDemoExitCode::InitializationFailed, "Window")) return EDemoExitCode::InitializationFailed;
    if (ShouldInject(EDemoStage::Runtime, EDemoExitCode::RuntimeUnavailable, "Runtime")) return EDemoExitCode::RuntimeUnavailable;
    if (ShouldInject(EDemoStage::Shader, EDemoExitCode::InitializationFailed, "TriangleShaders")) return EDemoExitCode::InitializationFailed;

    if (Configuration.RequiresNativeRuntime())
    {
#if !defined(STONER_VULKAN_RUNTIME_AVAILABLE) || !STONER_VULKAN_RUNTIME_AVAILABLE
        Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::RuntimeUnavailable, "Vulkan", "native Vulkan dependency unavailable");
        LifecycleState = EDemoLifecycleState::Failed;
        return EDemoExitCode::RuntimeUnavailable;
#elif !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
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

    Diagnostics.Add(EDemoStage::Window, EDemoExitCode::Success, "Window", Configuration.RequiresVisibleWindow() ? "native window ready" : "window not required");
    Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::Success, "Runtime", ToString(Configuration.RunMode));
    if (!ValidateShaderPayloads())
    {
        return FailInitialize(
            EDemoStage::Shader,
            EDemoExitCode::InitializationFailed,
            "TriangleShaders",
            "invalid stage, entry point, or checked-in SPIR-V payload");
    }
    Diagnostics.Add(EDemoStage::Shader, EDemoExitCode::Success, "TriangleShaders", "vertex and fragment main entry points validated");

    if (ShouldInject(EDemoStage::Upload, EDemoExitCode::InitializationFailed, "TriangleUpload"))
    {
        (void)Shutdown();
        return EDemoExitCode::InitializationFailed;
    }
    if (ShouldInject(EDemoStage::Pipeline, EDemoExitCode::InitializationFailed, "TrianglePipeline"))
    {
        (void)Shutdown();
        return EDemoExitCode::InitializationFailed;
    }

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
            return FailInitialize(
                EDemoStage::Pipeline,
                EDemoExitCode::InitializationFailed,
                "VisibleTriangle",
                "native presentation resources failed");
        }
    }
    Diagnostics.Add(EDemoStage::Upload, EDemoExitCode::Success, "TriangleUpload", "three-vertex RGB payload ready");
    Diagnostics.Add(EDemoStage::Pipeline, EDemoExitCode::Success, "TrianglePipeline", "triangle pipeline ready");

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
        Stoner::Backend::Vulkan::FVulkanNativeFrameBindings NativeBindings;
        const Stoner::RHI::ERHIResult AcquireResult = NativeContext->Context.AcquireVisibleFrame(NativeBindings);
        if (AcquireResult == Stoner::RHI::ERHIResult::ResizeRequired)
        {
            (void)NotifyDrawableExtent(0, 0, NowMilliseconds());
            continue;
        }
        if (AcquireResult != Stoner::RHI::ERHIResult::Success)
        {
            Diagnostics.Add(EDemoStage::Acquire, EDemoExitCode::FrameFailed, "FrameAcquire", "native swapchain image acquire failed");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::FrameFailed;
        }

        Stoner::Renderer::FForwardFrameExecutionBindings ExecutionBindings;
        ExecutionBindings.OutputTexture = NativeBindings.OutputTexture;
        ExecutionBindings.VertexBuffer = NativeBindings.VertexBuffer;
        ExecutionBindings.GraphicsPipeline = NativeBindings.GraphicsPipeline;
        ExecutionBindings.RenderPass = NativeBindings.RenderPass;
        ExecutionBindings.Framebuffer = NativeBindings.Framebuffer;
        ExecutionBindings.CommandBuffer = NativeBindings.CommandBuffer;
        const Stoner::Renderer::FForwardFramePlan FramePlan = BuildTriangleFramePlan(Width, Height);
        const Stoner::Renderer::FForwardFrameExecutionResult RecordResult =
            Stoner::Renderer::FForwardFrameExecutor().Execute(FramePlan, ExecutionBindings);
        if (!RecordResult.Succeeded())
        {
            Diagnostics.Add(EDemoStage::Record, EDemoExitCode::FrameFailed, "ForwardFrame", "Renderer failed to record native RHI frame bindings");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::FrameFailed;
        }
        const Stoner::RHI::ERHIResult PresentResult = NativeContext->Context.SubmitAndPresentVisibleFrame(NativeBindings);
        if (PresentResult == Stoner::RHI::ERHIResult::ResizeRequired)
        {
            (void)NotifyDrawableExtent(0, 0, NowMilliseconds());
            continue;
        }
        if (PresentResult != Stoner::RHI::ERHIResult::Success)
        {
            Diagnostics.Add(EDemoStage::Present, EDemoExitCode::FrameFailed, "FramePresent", "native frame submit or present failed");
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
    RunStartMilliseconds = NowMilliseconds();
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
    PresentationState.Reset();
    TriangleResources.Reset();
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
    bShutdownComplete = true;
    LifecycleState = EDemoLifecycleState::Stopped;
    Diagnostics.Add(EDemoStage::Shutdown, EDemoExitCode::Success, "DemoComposite", "reverse-order shutdown complete");
    return bInjectedShutdownFailure ? EDemoExitCode::InitializationFailed : EDemoExitCode::Success;
}

} // namespace Stoner::Demo
