#include "FStonerDemoApplication.h"

#include "Application/FWindow.h"
#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"
#include "RHI/ERHIRuntimeMode.h"
#include "Renderer/RendererMinimal.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
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

class FMountedFileSource final : public Stoner::Asset::IAssetSource
{
public:
    explicit FMountedFileSource(Stoner::Core::TArray<Stoner::Core::uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    Stoner::Asset::EAssetResult Read(
        Stoner::Core::uint64 Offset,
        Stoner::Core::usize MaximumBytes,
        Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size())
        {
            return Stoner::Asset::EAssetResult::MalformedSource;
        }
        const auto Count = std::min(
            MaximumBytes,
            Bytes_.size() - static_cast<Stoner::Core::usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return Stoner::Asset::EAssetResult::Success;
    }

    [[nodiscard]] std::size_t Size() const noexcept { return Bytes_.size(); }

private:
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes_;
};

class FMountedFileResolver final : public Stoner::Asset::IAssetResolver
{
public:
    explicit FMountedFileResolver(Stoner::Core::FString Root)
        : Root_(std::move(Root))
    {
    }

    Stoner::Asset::FAssetExtensionCapability GetCapability() const override
    {
        Stoner::Asset::FAssetParticipantId Participant;
        Stoner::Asset::FAssetProducerVersion Version;
        (void)Stoner::Asset::FAssetParticipantId::Create(
            "stoner.demo.content-resolver", Participant);
        (void)Stoner::Asset::FAssetProducerVersion::Create("023-v1", Version);
        return {
            Stoner::Asset::EAssetExtensionKind::Resolver,
            Participant,
            Version,
            100,
            {"content"},
            {},
            0};
    }

    Stoner::Asset::FAssetResolveResult Resolve(
        const Stoner::Asset::FAssetResolveRequest& Request) override
    {
        Stoner::Asset::FAssetResolveResult Result;
        Result.Descriptor.Location = Request.Location;
        if (Request.Location.GetScheme() != "content")
        {
            return Result;
        }
        const std::filesystem::path Relative(
            Request.Location.GetLocator().ToStdString());
        if (Relative.is_absolute() ||
            std::find(Relative.begin(), Relative.end(), "..") != Relative.end())
        {
            Result.Result = Stoner::Asset::EAssetResult::AccessDenied;
            return Result;
        }
        Stoner::Core::TArray<Stoner::Core::uint8> Bytes;
        const std::filesystem::path Path =
            std::filesystem::path(Root_.ToStdString()) / Relative;
        if (!Stoner::Core::FPlatformFileSystem::ReadFile(
                Path.generic_string().c_str(), Bytes))
        {
            Result.Result = Stoner::Asset::EAssetResult::NotFound;
            return Result;
        }
        auto Source =
            Stoner::Core::MakeShared<FMountedFileSource>(std::move(Bytes));
        Result.Descriptor.Size = Source->Size();
        Result.Source = Stoner::Asset::FAssetSourceLease(std::move(Source));
        Result.Result = Stoner::Asset::EAssetResult::Success;
        return Result;
    }

private:
    Stoner::Core::FString Root_;
};

class FPayloadLookup final : public Stoner::Asset::IShaderPayloadLookup
{
public:
    explicit FPayloadLookup(
        const Stoner::Core::TArray<
            Stoner::Core::TSharedPtr<const Stoner::Asset::FAssetPayload>>&
            Payloads)
        : Payloads_(Payloads)
    {
    }

    Stoner::Core::TSharedPtr<const Stoner::Asset::FShaderPayloadAsset> Find(
        const Stoner::Asset::FAssetId& Id) const override
    {
        for (const auto& Payload : Payloads_)
        {
            auto ShaderPayload = std::dynamic_pointer_cast<
                const Stoner::Asset::FShaderPayloadAsset>(Payload);
            if (ShaderPayload && ShaderPayload->GetId() == Id)
            {
                return ShaderPayload;
            }
        }
        return {};
    }

private:
    const Stoner::Core::TArray<
        Stoner::Core::TSharedPtr<const Stoner::Asset::FAssetPayload>>&
        Payloads_;
};

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
    using namespace Stoner;
    Asset::FAssetExtensionRegistry Extensions;
    Asset::FAssetRegistrationToken ResolverToken;
    if (Extensions.Register(
            Core::MakeShared<FMountedFileResolver>(
                Configuration.ShaderDirectory),
            ResolverToken) != Asset::EAssetResult::Success)
    {
        return false;
    }
    Asset::FAssetSourceLocator DefinitionLocation;
    if (Asset::FAssetSourceLocator::Create(
            "content",
            "Triangle.shader.json",
            DefinitionLocation) != Asset::EAssetResult::Success)
    {
        return false;
    }
    const Asset::FAssetResolveResult Definition =
        Asset::FAssetDispatch::Resolve(
            Extensions,
            {DefinitionLocation, {}});
    if (Definition.Result != Asset::EAssetResult::Success)
    {
        return false;
    }
    Asset::FAssetId ShaderId;
    (void)Asset::FAssetId::Create(
        "ShaderProgram",
        "Engine/Shaders/Triangle",
        std::nullopt,
        ShaderId);
    Asset::FMaterialShaderLoadRequest Request;
    Request.ExpectedId = ShaderId;
    Request.Extensions = &Extensions;
    Request.Descriptor = Definition.Descriptor;
    Request.Source = Definition.Source;
    const Asset::FMaterialShaderLoadResult Loaded =
        Asset::FMaterialShaderSourceLoader::Load(Request);
    if (!Loaded.Succeeded())
    {
        return false;
    }
    Core::TSharedPtr<const Asset::FShaderAsset> Program;
    for (const auto& Payload : Loaded.Payloads)
    {
        Program = std::dynamic_pointer_cast<
            const Asset::FShaderAsset>(Payload);
        if (Program) break;
    }
    if (!Program)
    {
        return false;
    }
    FPayloadLookup Lookup(Loaded.Payloads);
    Asset::FSelectedShaderProgram Selected;
    Asset::FShaderTargetRequest Target;
    Target.Backend = Asset::EShaderBackendFamily::Vulkan;
    Target.AcceptableProfiles = {"vulkan-1.3"};
    if (Asset::SelectShaderProgram(
            *Program,
            Target,
            Lookup,
            Selected) != Asset::EAssetResult::Success)
    {
        return false;
    }
    Renderer::FShaderAssetSnapshot Snapshot;
    if (Renderer::ConvertShaderAsset(
            {&Selected},
            Snapshot) != Renderer::EMaterialResult::Success ||
        Snapshot.ModuleDescriptions.size() != 2)
    {
        return false;
    }
    for (const auto& Module : Snapshot.ModuleDescriptions)
    {
        if (Module.Stage == RHI::ERHIShaderStage::Vertex)
            TriangleVertexShader = Module;
        else if (Module.Stage == RHI::ERHIShaderStage::Fragment)
            TriangleFragmentShader = Module;
    }
    bTriangleShadersLoaded =
        RHI::IsValidRHIShaderModuleDesc(TriangleVertexShader) &&
        RHI::IsValidRHIShaderModuleDesc(TriangleFragmentShader);
    return bTriangleShadersLoaded;
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
        CurrentDrawableWidth = Window->Value.GetDrawableWidth();
        CurrentDrawableHeight = Window->Value.GetDrawableHeight();
        if (CurrentDrawableWidth > 0 && CurrentDrawableHeight > 0 &&
            NativeContext->Context.PrepareVisibleTriangle(
                TriangleVertexShader, TriangleFragmentShader,
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
    if (NativeContext->Context.ExecuteOffscreenTriangle(
        TriangleVertexShader,
        TriangleFragmentShader) != Stoner::RHI::ERHIResult::Success)
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
                RecreateResult = NativeContext->Context.PrepareVisibleTriangle(
                    TriangleVertexShader,
                    TriangleFragmentShader,
                    Width,
                    Height);
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
