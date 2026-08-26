#include "FStonerDemoApplication.h"
#include "FProductionSubmissionHarness.h"
#include "Application/FWindow.h"
#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"
#include "FProductionContentComposition.h"
#include "FProductionContentDeferredExecution.h"
#include "FProductionContentRuntime.h"
#include "FProductionContentSession.h"
#include "FProductionWindowCaptureWriter.h"
#include "FStonerDemoWindowState.h"
#include "RHI/IRHICommandQueue.h"
#include "RHI/IRHIFence.h"
#include "RHI/ERHIRuntimeMode.h"
#include "Renderer/RendererMinimal.h"
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>

namespace Stoner::Demo
{
namespace
{
double NowMilliseconds()
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool IsRecoverablePresentationResult(
    EDemoGraphicsBackend Backend, Stoner::RHI::ERHIResult Result)
{
    if (Result == Stoner::RHI::ERHIResult::ResizeRequired) return true;
    return Backend == EDemoGraphicsBackend::Metal &&
        (Result == Stoner::RHI::ERHIResult::Unavailable ||
            Result == Stoner::RHI::ERHIResult::NotReady);
}

bool HasSameLiveObjects(
    const Stoner::RHI::FRHIRuntimeSnapshot& Left,
    const Stoner::RHI::FRHIRuntimeSnapshot& Right) noexcept
{
    return Left.LiveInstances == Right.LiveInstances &&
        Left.LiveDevices == Right.LiveDevices &&
        Left.LiveSurfaces == Right.LiveSurfaces &&
        Left.LiveSwapchains == Right.LiveSwapchains &&
        Left.LiveBuffers == Right.LiveBuffers &&
        Left.LiveTextures == Right.LiveTextures &&
        Left.LiveShaderModules == Right.LiveShaderModules &&
        Left.LivePipelines == Right.LivePipelines &&
        Left.LiveCommandBuffers == Right.LiveCommandBuffers &&
        Left.LiveSynchronizationObjects ==
            Right.LiveSynchronizationObjects;
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
    {
        for (const auto& Payload : Payloads)
        {
            auto ShaderPayload = std::dynamic_pointer_cast<
                const Stoner::Asset::FShaderPayloadAsset>(Payload);
            if (ShaderPayload) Payloads_.push_back(std::move(ShaderPayload));
        }
    }

    explicit FPayloadLookup(
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<
            const Stoner::Asset::FShaderPayloadAsset>>& Payloads)
        : Payloads_(Payloads)
    {
    }

    Stoner::Core::TSharedPtr<const Stoner::Asset::FShaderPayloadAsset> Find(
        const Stoner::Asset::FAssetId& Id) const override
    {
        for (const auto& Payload : Payloads_)
        {
            if (Payload && Payload->GetId() == Id)
            {
                return Payload;
            }
        }
        return {};
    }

private:
    Stoner::Core::TArray<
        Stoner::Core::TSharedPtr<const Stoner::Asset::FShaderPayloadAsset>>
        Payloads_;
};

} // namespace

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

FStonerDemoApplication::FStonerDemoApplication(
    FDemoConfiguration InConfiguration,
    Stoner::Core::TSharedPtr<IDemoBackendFactory> InBackendFactory)
    : Configuration(std::move(InConfiguration)),
      ValidationMonitor(Configuration),
      BackendFactory(std::move(InBackendFactory))
{
    if (!BackendFactory)
    {
        BackendFactory = Stoner::Core::MakeShared<FDemoBackendFactory>();
    }
}

FStonerDemoApplication::~FStonerDemoApplication()
{
    if (!bShutdownComplete) (void)Shutdown();
}

bool FStonerDemoApplication::ValidateShaderPayloads()
{
    using namespace Stoner;
    if (Configuration.GraphicsBackend == EDemoGraphicsBackend::Metal &&
        Configuration.RequiresNativeRuntime())
    {
        return LoadStrictCookedMetalShaderPayloads();
    }
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

bool FStonerDemoApplication::LoadStrictCookedMetalShaderPayloads()
{
    using namespace Stoner;
    Core::TArray<Core::uint8> ProfileBytes;
    Asset::FAssetTargetProfileEvidence TargetEvidence;
    if (!Core::FPlatformFileSystem::ReadFile(
            Configuration.TargetProfilePath, ProfileBytes) ||
        Asset::FAssetCookContractCodec::ParseTargetProfile(
            ProfileBytes, TargetEvidence) != Asset::EAssetResult::Success ||
        TargetEvidence.Profile.Platform != Asset::EAssetTargetPlatform::MacOS ||
        TargetEvidence.Profile.GraphicsBackend !=
            Asset::EAssetGraphicsBackend::Metal)
        return false;

    Core::FString SelectedProfile;
    for (const auto& Choice : TargetEvidence.Profile.ShaderPayloadChoices)
    {
        if (Choice.Backend == Asset::EAssetGraphicsBackend::Metal &&
            Choice.Format == Asset::EAssetShaderPayloadFormat::MetalLibrary)
        {
            SelectedProfile = Choice.Profile;
            break;
        }
    }
    if (SelectedProfile.IsEmpty()) return false;

    std::error_code FileError;
    std::filesystem::create_directories(
        Configuration.LeaseCoordinationRoot.ToStdString(), FileError);
    if (FileError) return false;

    Asset::FAssetManagerConfig ManagerConfig;
    ManagerConfig.Mode = Asset::EAssetManagerMode::StrictCooked;
    ManagerConfig.ExtensionRegistry =
        Core::MakeShared<Asset::FAssetExtensionRegistry>();
    ManagerConfig.PublicationRoot = Configuration.CookedPublicationRoot;
    ManagerConfig.LeaseCoordinationRoot = Configuration.LeaseCoordinationRoot;
    ManagerConfig.TargetEvidence =
        Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(
            TargetEvidence);
    Core::TSharedPtr<Asset::FAssetManager> Manager;
    Asset::FAssetDiagnosticList AssetDiagnostics;
    if (Asset::FAssetManager::Create(
            ManagerConfig, Manager, AssetDiagnostics) !=
            Asset::EAssetResult::Success || !Manager)
        return false;

    const auto WaitReady = [&Manager](
        Asset::FAssetRequestHandle Request) -> bool
    {
        for (int Attempt = 0; Attempt < 6000; ++Attempt)
        {
            Asset::FAssetRequestSnapshot Snapshot;
            if (Manager->Query(Request, Snapshot) !=
                Asset::EAssetResult::Success)
                return false;
            if (Snapshot.State == Asset::EAssetRequestState::Ready)
                return true;
            if (Snapshot.State == Asset::EAssetRequestState::Failed ||
                Snapshot.State == Asset::EAssetRequestState::Cancelled)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };
    const auto ShutdownManager = [&Manager](
        const Core::TArray<Asset::FAssetRequestHandle>& Requests)
    {
        for (const auto Request : Requests)
            if (Request.IsValid()) (void)Manager->ReleaseRequest(Request);
        (void)Manager->Shutdown();
        Manager.reset();
    };

    Asset::FAssetId ProgramId;
    if (Asset::FAssetId::Create(
            "ShaderProgram", "Engine/Shaders/Triangle", std::nullopt,
            ProgramId) != Asset::EAssetResult::Success)
    {
        (void)Manager->Shutdown();
        return false;
    }
    Core::TArray<Asset::FAssetRequestHandle> Requests;
    Asset::FAssetRequestHandle ProgramRequest;
    Asset::TAssetHandle<Asset::FShaderAsset> ProgramHandle;
    if (Manager->Request<Asset::FShaderAsset>(ProgramId, ProgramRequest) !=
            Asset::EAssetResult::Success ||
        !WaitReady(ProgramRequest) ||
        Manager->GetResult(ProgramRequest, ProgramHandle) !=
            Asset::EAssetResult::Success || !ProgramHandle.IsValid())
    {
        Requests.push_back(ProgramRequest);
        ShutdownManager(Requests);
        return false;
    }
    Requests.push_back(ProgramRequest);

    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> Payloads;
    for (const auto& Variant : ProgramHandle->GetDesc().Variants)
    {
        if (!Variant.Permutation.Flags.empty()) continue;
        for (const auto& Reference : Variant.Payloads)
        {
            const bool bExactMetal =
                Reference.Backend == Asset::EShaderBackendFamily::Metal &&
                Reference.Format == Asset::EShaderPayloadFormat::MetalLibrary;
            const bool bDerivedMetal =
                Reference.Backend == Asset::EShaderBackendFamily::Vulkan &&
                Reference.Format == Asset::EShaderPayloadFormat::SPIRV;
            if (!bExactMetal && !bDerivedMetal)
                continue;
            const auto& PayloadId = Reference.Payload.GetId();
            if (!PayloadId) continue;
            Asset::FAssetRequestHandle PayloadRequest;
            Asset::TAssetHandle<Asset::FShaderPayloadAsset> PayloadHandle;
            if (Manager->Request<Asset::FShaderPayloadAsset>(
                    *PayloadId, PayloadRequest) != Asset::EAssetResult::Success ||
                !WaitReady(PayloadRequest) ||
                Manager->GetResult(PayloadRequest, PayloadHandle) !=
                    Asset::EAssetResult::Success || !PayloadHandle.IsValid())
            {
                Requests.push_back(PayloadRequest);
                ShutdownManager(Requests);
                return false;
            }
            Requests.push_back(PayloadRequest);
            Payloads.push_back(
                Core::MakeShared<const Asset::FShaderPayloadAsset>(
                    *PayloadHandle));
        }
        break;
    }
    FPayloadLookup Lookup(Payloads);
    Asset::FShaderTargetRequest Target;
    Target.Backend = Asset::EShaderBackendFamily::Metal;
    Target.CpuArchitecture = TargetEvidence.Profile.CpuArchitecture;
    Target.AcceptableProfiles = {SelectedProfile};
    Asset::FSelectedShaderProgram Selected;
    Renderer::FShaderAssetSnapshot Snapshot;
    const bool bSelected = Payloads.size() == 2 &&
        Asset::SelectShaderProgram(
            *ProgramHandle, Target, Lookup, Selected) ==
            Asset::EAssetResult::Success &&
        Renderer::ConvertShaderAsset({&Selected}, Snapshot) ==
            Renderer::EMaterialResult::Success &&
        Snapshot.ModuleDescriptions.size() == 2;
    ProgramHandle.Reset();
    Selected.Stages.clear();
    Payloads.clear();
    ShutdownManager(Requests);
    if (!bSelected) return false;

    TriangleVertexShader = {};
    TriangleFragmentShader = {};
    for (auto& Module : Snapshot.ModuleDescriptions)
    {
        if (Module.Stage == RHI::ERHIShaderStage::Vertex)
            TriangleVertexShader = std::move(Module);
        else if (Module.Stage == RHI::ERHIShaderStage::Fragment)
            TriangleFragmentShader = std::move(Module);
    }
    bTriangleShadersLoaded =
        RHI::IsValidRHIShaderModuleDesc(TriangleVertexShader) &&
        TriangleVertexShader.Payload.Format ==
            RHI::ERHIShaderPayloadFormat::MetalLibrary &&
        RHI::IsValidRHIShaderModuleDesc(TriangleFragmentShader) &&
        TriangleFragmentShader.Payload.Format ==
            RHI::ERHIShaderPayloadFormat::MetalLibrary;
    return bTriangleShadersLoaded;
}

bool FStonerDemoApplication::ShouldInject(EDemoStage Stage, EDemoExitCode Code, const char* Subject)
{
    if (!bHasFailureInjection || FailureInjectionStage != Stage) return false;
    Diagnostics.Add(Stage, Code, Subject, "injected failure");
    LifecycleState = EDemoLifecycleState::Failed;
    return true;
}

EDemoExitCode FStonerDemoApplication::InitializeProductionContent()
{
    using namespace Stoner;
    if (!BackendRuntime || !BackendRuntime->GetDevice())
        return FailInitialize(EDemoStage::Runtime,
            EDemoExitCode::RuntimeUnavailable, "ProductionDevice",
            "production content requires an active native RHI device");

    Core::TArray<Core::uint8> ProfileBytes;
    Asset::FAssetTargetProfileEvidence TargetEvidence;
    Asset::FAssetDigest Generation;
    if (!Core::FPlatformFileSystem::ReadFile(
            Configuration.TargetProfilePath, ProfileBytes) ||
        Asset::FAssetCookContractCodec::ParseTargetProfile(
            ProfileBytes, TargetEvidence) != Asset::EAssetResult::Success ||
        Asset::FAssetDigest::ParseLowerHex(
            Configuration.StrictGeneration, Generation) !=
            Asset::EAssetResult::Success)
        return FailInitialize(EDemoStage::Shader,
            EDemoExitCode::InitializationFailed, "ProductionContract",
            "target profile or strict generation is invalid");
    ProductionRuntime = std::make_unique<FProductionContentRuntime>();
    if (!ProductionContentSession)
        ProductionContentSession =
            std::make_unique<FProductionContentSession>();
    FProductionContentSessionConfig SessionConfig;
    SessionConfig.PublicationRoot = Configuration.CookedPublicationRoot;
    SessionConfig.LeaseCoordinationRoot = Configuration.LeaseCoordinationRoot;
    SessionConfig.RootAssetIdentity = Configuration.ProductionRoot;
    SessionConfig.ExpectedGeneration = Generation;
    SessionConfig.TargetEvidence = Core::MakeShared<
        const Asset::FAssetTargetProfileEvidence>(TargetEvidence);
    SessionConfig.WorkerCount = SelectProductionContentWorkerCount(
        Configuration.WorkloadRevision,
        Configuration.ProductionLifecycleCycles,
        TargetEvidence.Profile.GraphicsBackend,
        TargetEvidence.Profile.CpuArchitecture);
    SessionConfig.bLoadRootClosureFirst =
        ShouldLoadProductionRootClosureFirst(
            Configuration.WorkloadRevision,
            Configuration.ProductionLifecycleCycles);
    SessionConfig.bReuseCookedEnvelopeAuthentication =
        ShouldReuseProductionCookedEnvelopeAuthentication(
            Configuration.WorkloadRevision,
            Configuration.ProductionLifecycleCycles);
    const Asset::EAssetResult SessionResult = ProductionContentSession->Load(
        SessionConfig, ProductionRuntime->LoadedClosure);
    if (SessionResult != Asset::EAssetResult::Success)
        return FailInitialize(EDemoStage::Upload,
            EDemoExitCode::InitializationFailed, "ProductionStrictSession",
            ProductionContentSession->Inspect().FirstFailure.IsEmpty()
                ? "strict cooked closure failed"
                : ProductionContentSession->Inspect().FirstFailure.CStr());

    Renderer::FStaticModelRealizationRequest Request;
    Request.Device = BackendRuntime->GetDevice();
    Request.Model = ProductionRuntime->LoadedClosure.Model;
    Request.Dependencies = ProductionRuntime->LoadedClosure.Dependencies;
    Request.TargetEvidence = SessionConfig.TargetEvidence;
    if (Request.Dependencies.Textures.empty())
        return FailInitialize(EDemoStage::Upload,
            EDemoExitCode::InitializationFailed, "ProductionTextures",
            "production closure has no cooked texture");
    for (const auto& Texture : Request.Dependencies.Textures)
    {
        Request.TextureTargetProfiles.push_back({
            Texture->GetId(),
            Renderer::FTextureTargetProfile::DesktopDefault(
                Texture->GetInfo())});
    }
    Request.RenderTargets.SampleCount = RHI::ERHISampleCount::One;
    Request.RenderTargets.ColorFormats = {
        RHI::ERHIFormat::R8G8B8A8_UNorm,
        RHI::ERHIFormat::R16G16B16A16_Float,
        RHI::ERHIFormat::R16G16B16A16_Float};
    Request.RenderTargets.DepthStencilFormat = RHI::ERHIFormat::D32_Float;
    const RHI::ERHIResult DeferredRealized =
        Renderer::FStaticModelRealizer::Realize(
            Request, ProductionRuntime->DeferredRenderSnapshot,
            ProductionRuntime->DeferredRealizationInspection);
    if (DeferredRealized == RHI::ERHIResult::Success)
    {
        ProductionRuntime->ForwardRenderSnapshot =
            ProductionRuntime->DeferredRenderSnapshot;
        ProductionRuntime->ForwardRealizationInspection =
            ProductionRuntime->DeferredRealizationInspection;
    }
    if (DeferredRealized != RHI::ERHIResult::Success ||
        !ProductionRuntime->DeferredRenderSnapshot ||
        !ProductionRuntime->ForwardRenderSnapshot)
        return FailInitialize(EDemoStage::Pipeline,
            EDemoExitCode::InitializationFailed, "ProductionRealization",
            ProductionRuntime->DeferredRealizationInspection.FirstFailure.Reason.IsEmpty()
                ? "aggregate Renderer realization failed"
                : ProductionRuntime->DeferredRealizationInspection.FirstFailure.Reason.CStr());

    FProductionContentCompositionConfig CompositionConfig;
    CompositionConfig.WorkloadRevision = Configuration.WorkloadRevision;
    CompositionConfig.Width = Configuration.GetProductionRenderWidth();
    CompositionConfig.Height = Configuration.GetProductionRenderHeight();
    Core::FString CompositionReason;
    if (!FProductionContentCompositionBuilder::Build(
            ProductionRuntime->DeferredRenderSnapshot, CompositionConfig,
            ProductionRuntime->Composition, &CompositionReason))
        return FailInitialize(EDemoStage::Pipeline,
            EDemoExitCode::InitializationFailed, "ProductionComposition",
            CompositionReason.CStr());

    if (FProductionContentDeferredExecutionBuilder::Build(
            Request.Device, *ProductionRuntime->DeferredRenderSnapshot,
            ProductionRuntime->Composition,
            ProductionRuntime->LoadedClosure.RenderShaders,
            ProductionRuntime->LoadedClosure.RenderShaderPayloads,
            TargetEvidence, ProductionRuntime->DeferredResources,
            &CompositionReason) != RHI::ERHIResult::Success)
        return FailInitialize(EDemoStage::Pipeline,
            EDemoExitCode::InitializationFailed,
            "ProductionDeferredExecution", CompositionReason.CStr());

    Renderer::FDeferredRendererConfiguration DeferredConfig;
    DeferredConfig.bEnableValidationReadback = true;
    Renderer::FDeferredFramePlan DeferredPlan;
    if (Renderer::FForwardRenderer().PrepareFrame(
            ProductionRuntime->Composition.ForwardInputs,
            ProductionRuntime->ForwardPlan) !=
            Renderer::EForwardResult::Success ||
        Renderer::FDeferredRenderer(DeferredConfig).PrepareFrame(
            ProductionRuntime->Composition.DeferredInputs,
            DeferredPlan) != Renderer::EDeferredResult::Success ||
        !PrepareProductionForwardSmoke(
            *Request.Device, *ProductionRuntime->ForwardRenderSnapshot,
            ProductionRuntime->ForwardPlan, DeferredPlan,
            ProductionRuntime->ForwardBindings, &CompositionReason))
        return FailInitialize(EDemoStage::Pipeline,
            EDemoExitCode::InitializationFailed,
            "ProductionForwardExecution", CompositionReason.CStr());

    Diagnostics.Add(EDemoStage::Upload, EDemoExitCode::Success,
        "ProductionStrictSession",
        "strict generation and complete dependency closure loaded");
    Diagnostics.Add(EDemoStage::Pipeline, EDemoExitCode::Success,
        "ProductionRealization",
        "aggregate render snapshot and backend-neutral composition published");
    return EDemoExitCode::Success;
}

FDemoProductionLifecycleCounters
FStonerDemoApplication::ReleaseProductionContentCycle()
{
    FDemoProductionLifecycleCounters Counters;
    if (!ProductionRuntime || !ProductionContentSession || !BackendRuntime)
    {
        Counters.AssetOwners = 1;
        Counters.RendererOwners = 1;
        Counters.RHIObjects = 1;
        Counters.NativeObjects = 1;
        Counters.bStaleHandleRejected = false;
        return Counters;
    }

    const Core::TWeakPtr<const Renderer::FStaticModelRenderSnapshot>
        StaleDeferredSnapshot = ProductionRuntime->DeferredRenderSnapshot;
    const Core::TWeakPtr<const Renderer::FStaticModelRenderSnapshot>
        StaleForwardSnapshot = ProductionRuntime->ForwardRenderSnapshot;
    ReleaseProductionForwardSmoke(ProductionRuntime->ForwardBindings);
    ProductionRuntime->DeferredResources.Release();
    ProductionRuntime->ForwardPlan = {};
    ProductionRuntime->Composition = {};
    ProductionRuntime->DeferredRenderSnapshot.reset();
    ProductionRuntime->ForwardRenderSnapshot.reset();
    ProductionRuntime->LoadedClosure = {};
    (void)ProductionContentSession->Shutdown();
    const FProductionContentSessionInspection SessionInspection =
        ProductionContentSession->Inspect();
    ProductionExecutionInspection.bCookedEnvelopeAuthenticationReused =
        SessionInspection.CookedEnvelopeAuthentication.ReuseHits > 0;
    ProductionExecutionInspection.bCookedGenerationValidationReused =
        ProductionExecutionInspection.bCookedGenerationValidationReused ||
        SessionInspection.bGenerationValidationReused;
    ProductionRuntime.reset();

    const Asset::FAssetManagerInspection& Manager =
        SessionInspection.Manager;
    Counters.AssetOwners =
        static_cast<Core::uint64>(Manager.ActiveOperations) +
        Manager.CachedAssets + Manager.ExternalHandleRetentions +
        Manager.RequestRetentions + Manager.RequiredDependencyRetentions +
        Manager.CompletionReservations + Manager.QueuedCompletions +
        Manager.Requests.size() + Manager.Operations.size();
    Counters.RendererOwners =
        (StaleDeferredSnapshot.expired() ? 0 : 1) +
        (StaleForwardSnapshot.expired() ? 0 : 1);

    const RHI::FRHIRuntimeSnapshot Released = BackendRuntime->GetSnapshot();
    const bool bRuntimeReturned =
        HasSameLiveObjects(Released, ProductionRuntimeBaseline);
    Counters.RHIObjects = bRuntimeReturned ? 0 :
        Released.GetTotalLiveObjectCount();
    Counters.NativeObjects = bRuntimeReturned ? 0 : 1;
    Counters.PresentationObjects =
        Released.LiveSurfaces == ProductionRuntimeBaseline.LiveSurfaces &&
        Released.LiveSwapchains == ProductionRuntimeBaseline.LiveSwapchains
        ? 0 : static_cast<Core::uint64>(Released.LiveSurfaces) +
            Released.LiveSwapchains;
    Counters.bStaleHandleRejected =
        SessionInspection.bStaleHandleRejected &&
        StaleDeferredSnapshot.expired() && StaleForwardSnapshot.expired();
    return Counters;
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
    if (Configuration.Workload == EDemoWorkload::Triangle &&
        ShouldInject(EDemoStage::Shader,
            EDemoExitCode::InitializationFailed, "TriangleShaders"))
        return EDemoExitCode::InitializationFailed;

    if (Configuration.RequiresNativeRuntime())
    {
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
        if (Configuration.RequiresVisibleWindow())
        {
            Diagnostics.Add(EDemoStage::Window, EDemoExitCode::RuntimeUnavailable, "GLFW", "native window dependency unavailable");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::RuntimeUnavailable;
        }
#endif
        FDemoBackendCreateResult Created =
            BackendFactory->Create(Configuration.GraphicsBackend);
        if (!Created.Succeeded())
        {
            Diagnostics.Add(
                EDemoStage::Runtime,
                EDemoExitCode::RuntimeUnavailable,
                ToString(Configuration.GraphicsBackend),
                Created.FailureReason.IsEmpty()
                    ? "requested backend factory unavailable"
                    : Created.FailureReason.CStr());
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::RuntimeUnavailable;
        }
        BackendRuntime = std::move(Created.Runtime);
    }

    if (Configuration.RequiresVisibleWindow())
    {
        Window = std::make_unique<FWindowHolder>();
        Stoner::Application::FWindowDesc Desc;
        Desc.Title = Configuration.Workload == EDemoWorkload::Triangle
            ? "Stoner Graphics Lab - Triangle Demo"
            : "Stoner Graphics Lab - Production Content";
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
    }

    if (Configuration.RequiresNativeRuntime())
    {
        const Stoner::Core::FPlatformWindow PlatformWindow = Window
            ? Window->Value.GetPlatformWindow()
            : Stoner::Core::FPlatformWindow{};
        const Stoner::RHI::ERHIResult NativeResult = BackendRuntime->Initialize(
            Configuration.RunMode,
            PlatformWindow,
            Configuration.MaxFramesInFlight,
            Configuration.bEnableValidationLayers);
        const Stoner::RHI::FRHIRuntimeSnapshot NativeSnapshot =
            BackendRuntime->GetSnapshot();
        if (NativeResult != Stoner::RHI::ERHIResult::Success ||
            !NativeSnapshot.ProvesNativeExecution())
        {
            std::string Reason =
                NativeResult != Stoner::RHI::ERHIResult::Success
                ? "native runtime initialization failed"
                : "native runtime snapshot proof unavailable";
            Reason += "; requested=";
            Reason += Stoner::RHI::ToString(NativeSnapshot.RequestedMode);
            Reason += "; object=";
            Reason += std::to_string(
                static_cast<int>(NativeSnapshot.ObjectMode)).c_str();
            Reason += "; instances=";
            Reason += std::to_string(NativeSnapshot.LiveInstances).c_str();
            Reason += "; devices=";
            Reason += std::to_string(NativeSnapshot.LiveDevices).c_str();
            Diagnostics.Add(
                EDemoStage::Runtime,
                EDemoExitCode::RuntimeUnavailable,
                ToString(Configuration.GraphicsBackend),
                Reason.c_str());
            (void)BackendRuntime->Shutdown();
            BackendRuntime.reset();
            if (Window)
            {
                (void)Window->Value.Destroy();
                Window.reset();
            }
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::RuntimeUnavailable;
        }
        ProductionRuntimeBaseline = BackendRuntime->GetSnapshot();
    }

    Diagnostics.Add(EDemoStage::Window, EDemoExitCode::Success, "Window", Configuration.RequiresVisibleWindow() ? "native window ready" : "window not required");
    Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::Success, "Runtime", ToString(Configuration.RunMode));
    if (Configuration.Workload == EDemoWorkload::ProductionContent)
    {
        ProductionSubmissionHarness =
            Stoner::Core::MakeUnique<FProductionSubmissionHarness>();
        if (ProductionSubmissionHarness->Initialize(
                BackendRuntime->GetDevice()) != Stoner::RHI::ERHIResult::Success)
            return FailInitialize(
                EDemoStage::Pipeline,
                EDemoExitCode::InitializationFailed,
                "ProductionSubmissionHarness",
                "persistent production queue or fence creation failed");
        if (Configuration.bVisibleCapture ||
            Configuration.bProductionCameraPreview)
        {
            CurrentDrawableWidth = Window->Value.GetDrawableWidth();
            CurrentDrawableHeight = Window->Value.GetDrawableHeight();
            if (CurrentDrawableWidth == 0 || CurrentDrawableHeight == 0 ||
                BackendRuntime->PrepareProductionPresentation(
                    CurrentDrawableWidth, CurrentDrawableHeight) !=
                    Stoner::RHI::ERHIResult::Success)
                return FailInitialize(
                    EDemoStage::Pipeline,
                    EDemoExitCode::InitializationFailed,
                    "ProductionPresentation",
                    "native production presentation resources failed");
        }
        ProductionRuntimeBaseline = BackendRuntime->GetSnapshot();
        const EDemoExitCode ProductionResult = InitializeProductionContent();
        if (ProductionResult != EDemoExitCode::Success)
            return ProductionResult;
    }
    else if (!ValidateShaderPayloads())
    {
        return FailInitialize(
            EDemoStage::Shader,
            EDemoExitCode::InitializationFailed,
            "TriangleShaders",
            "invalid stage, entry point, or checked-in SPIR-V payload");
    }
    if (Configuration.Workload == EDemoWorkload::Triangle)
        Diagnostics.Add(EDemoStage::Shader, EDemoExitCode::Success,
            "TriangleShaders",
            "vertex and fragment main entry points validated");

    if (Configuration.Workload == EDemoWorkload::Triangle &&
        ShouldInject(EDemoStage::Upload,
            EDemoExitCode::InitializationFailed, "TriangleUpload"))
    {
        (void)Shutdown();
        return EDemoExitCode::InitializationFailed;
    }
    if (Configuration.Workload == EDemoWorkload::Triangle &&
        ShouldInject(EDemoStage::Pipeline,
            EDemoExitCode::InitializationFailed, "TrianglePipeline"))
    {
        (void)Shutdown();
        return EDemoExitCode::InitializationFailed;
    }

    if (Configuration.Workload == EDemoWorkload::Triangle &&
        Configuration.RequiresVisibleWindow())
    {
        CurrentDrawableWidth = Window->Value.GetDrawableWidth();
        CurrentDrawableHeight = Window->Value.GetDrawableHeight();
        if (CurrentDrawableWidth > 0 && CurrentDrawableHeight > 0 &&
            BackendRuntime->PrepareTriangle(
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
    if (Configuration.Workload == EDemoWorkload::Triangle)
    {
        Diagnostics.Add(EDemoStage::Upload, EDemoExitCode::Success,
            "TriangleUpload", "three-vertex RGB payload ready");
        Diagnostics.Add(EDemoStage::Pipeline, EDemoExitCode::Success,
            "TrianglePipeline", "triangle pipeline ready");
        TriangleResources.bInitialized = true;
    }
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
    if (!BackendRuntime) return EDemoExitCode::RuntimeUnavailable;
    LifecycleState = EDemoLifecycleState::Running;
    const Stoner::Renderer::FForwardFramePlan FramePlan =
        BuildTriangleFramePlan(64, 64);
    if (BackendRuntime->ExecuteOffscreenTriangle(
        FramePlan,
        TriangleVertexShader,
        TriangleFragmentShader) != Stoner::RHI::ERHIResult::Success)
    {
        Diagnostics.Add(EDemoStage::Submit, EDemoExitCode::FrameFailed, "NativeOffscreenTriangle", "native offscreen command submission failed");
        LifecycleState = EDemoLifecycleState::Failed;
        return EDemoExitCode::FrameFailed;
    }
    const Stoner::RHI::FRHIRuntimeSnapshot Snapshot = BackendRuntime->GetSnapshot();
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

EDemoExitCode FStonerDemoApplication::DriveValidationWindowCycle()
{
    using Stoner::Application::EApplicationResult;
    if (!Window || Configuration.ValidationLifecycleCycles == 0)
        return EDemoExitCode::Success;
    const Stoner::Core::uint32 CycleWindowFrames = std::min(
        1000u, std::max(1u, Configuration.FrameBudget / 3u));
    const Stoner::Core::uint32 Interval = std::max(
        1u, CycleWindowFrames /
            (Configuration.ValidationLifecycleCycles + 1u));
    switch (ValidationWindowCycleState)
    {
    case EDemoValidationWindowCycleState::Idle:
        if (ValidationWindowCyclesStarted >=
                Configuration.ValidationLifecycleCycles ||
            CompletedFrames < (ValidationWindowCyclesStarted + 1u) * Interval)
            return EDemoExitCode::Success;
        ValidationWindowExpectedWidth =
            (ValidationWindowCyclesStarted % 2u == 0u)
                ? std::max(320u, Configuration.ClientWidth - 160u)
                : Configuration.ClientWidth;
        ValidationWindowExpectedHeight =
            (ValidationWindowCyclesStarted % 2u == 0u)
                ? std::max(240u, Configuration.ClientHeight - 90u)
                : Configuration.ClientHeight;
        if (Window->Value.SetClientSize(
                ValidationWindowExpectedWidth,
                ValidationWindowExpectedHeight) != EApplicationResult::Success)
            return EDemoExitCode::ValidationFailed;
        ValidationWindowCycleState =
            EDemoValidationWindowCycleState::WaitingForResize;
        return EDemoExitCode::Success;
    case EDemoValidationWindowCycleState::WaitingForResize:
        if (Window->Value.GetClientWidth() != ValidationWindowExpectedWidth ||
            Window->Value.GetClientHeight() != ValidationWindowExpectedHeight)
            return EDemoExitCode::Success;
        if (Window->Value.Minimize() != EApplicationResult::Success)
            return EDemoExitCode::ValidationFailed;
        ValidationWindowCycleState =
            EDemoValidationWindowCycleState::WaitingForMinimize;
        return EDemoExitCode::Success;
    case EDemoValidationWindowCycleState::WaitingForMinimize:
        if (!Window->Value.IsMinimized()) return EDemoExitCode::Success;
        if (Window->Value.Restore() != EApplicationResult::Success)
            return EDemoExitCode::ValidationFailed;
        ValidationWindowCycleState =
            EDemoValidationWindowCycleState::WaitingForRestore;
        return EDemoExitCode::Success;
    case EDemoValidationWindowCycleState::WaitingForRestore:
        if (Window->Value.IsMinimized() || !Window->Value.HasDrawableArea())
            return EDemoExitCode::Success;
        ++ValidationWindowCyclesStarted;
        ValidationWindowCycleState = EDemoValidationWindowCycleState::Idle;
        return EDemoExitCode::Success;
    }
    return EDemoExitCode::ValidationFailed;
}

EDemoExitCode FStonerDemoApplication::RunVisible()
{
    if (!BackendRuntime || !Window) return EDemoExitCode::RuntimeUnavailable;
    LifecycleState = EDemoLifecycleState::Running;
    while (!Window->Value.IsCloseRequested() &&
        (!Configuration.IsBounded() || CompletedFrames < Configuration.FrameBudget))
    {
        (void)Window->Value.PollEvents();
        (void)Window->Value.PollInputEvents();
        if (Window->Value.IsCloseRequested()) break;
        if (DriveValidationWindowCycle() != EDemoExitCode::Success)
        {
            Diagnostics.Add(EDemoStage::Present,
                EDemoExitCode::ValidationFailed,
                "ValidationWindowCycle",
                "window lifecycle command failed");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::ValidationFailed;
        }
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
                RecreateResult = BackendRuntime->PrepareTriangle(
                    TriangleVertexShader,
                    TriangleFragmentShader,
                    Width,
                    Height);
                if (RecreateResult == Stoner::RHI::ERHIResult::Success) PresentationState.bInitialized = true;
            }
            else RecreateResult = BackendRuntime->RecreatePresentation(Width, Height);
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
        FDemoBackendFrame BackendFrame;
        const Stoner::RHI::ERHIResult AcquireResult = BackendRuntime->AcquireFrame(BackendFrame);
        if (IsRecoverablePresentationResult(
                Configuration.GraphicsBackend, AcquireResult))
        {
            (void)NotifyDrawableExtent(0, 0, NowMilliseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }
        if (AcquireResult != Stoner::RHI::ERHIResult::Success)
        {
            Diagnostics.Add(EDemoStage::Acquire, EDemoExitCode::FrameFailed, "FrameAcquire", "native swapchain image acquire failed");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::FrameFailed;
        }

        Stoner::Renderer::FForwardFrameExecutionBindings ExecutionBindings;
        ExecutionBindings = BackendFrame.ExecutionBindings;
        const Stoner::Renderer::FForwardFramePlan FramePlan = BuildTriangleFramePlan(Width, Height);
        const Stoner::Renderer::FForwardFrameExecutionResult RecordResult =
            Stoner::Renderer::FForwardFrameExecutor().Execute(FramePlan, ExecutionBindings);
        if (!RecordResult.Succeeded())
        {
            Diagnostics.Add(EDemoStage::Record, EDemoExitCode::FrameFailed, "ForwardFrame", "Renderer failed to record native RHI frame bindings");
            LifecycleState = EDemoLifecycleState::Failed;
            return EDemoExitCode::FrameFailed;
        }
        const Stoner::RHI::ERHIResult PresentResult = BackendRuntime->SubmitFrame(BackendFrame);
        if (IsRecoverablePresentationResult(
                Configuration.GraphicsBackend, PresentResult))
        {
            (void)NotifyDrawableExtent(0, 0, NowMilliseconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
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
            !ValidationMonitor.Sample(CompletedFrames, BackendRuntime->GetSnapshot()))
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
        if (Configuration.Workload == EDemoWorkload::ProductionContent)
            Result = Configuration.bProductionCameraPreview
                ? RunProductionCameraPreview() : RunProductionContent();
        else if (Configuration.RunMode == EDemoRunMode::DeterministicHeadless)
            Result = RunDeterministic();
        else if (Configuration.RunMode == EDemoRunMode::NativeHeadless)
            Result = RunNativeHeadless();
        else Result = RunVisible();
    }

    const bool bUsesLegacyFrameValidation =
        Configuration.Workload == EDemoWorkload::Triangle &&
        Configuration.IsBounded();
    ValidationMonitor.SetRequestedFrames(
        bUsesLegacyFrameValidation ? Configuration.FrameBudget : CompletedFrames);
    ValidationMonitor.SetCompletedFrames(CompletedFrames);
    const EDemoExitCode ShutdownResult = Shutdown();
    if (Result == EDemoExitCode::Success && ShutdownResult != EDemoExitCode::Success) Result = ShutdownResult;

    ValidationMonitor.SetRuntimeSnapshot({});
    if (Result == EDemoExitCode::Success && bUsesLegacyFrameValidation &&
        !ValidationMonitor.Evaluate())
    {
        Diagnostics.Add(EDemoStage::Memory, EDemoExitCode::ValidationFailed, "Endurance", "memory or final resource gate failed");
        Result = EDemoExitCode::ValidationFailed;
    }
    if (bUsesLegacyFrameValidation &&
        ShouldInject(EDemoStage::Report,
            EDemoExitCode::ReportFailed, "ValidationReport"))
    {
        if (Result == EDemoExitCode::Success) Result = EDemoExitCode::ReportFailed;
    }
    else if (bUsesLegacyFrameValidation &&
        !ValidationMonitor.WriteReport(Diagnostics) &&
        Result == EDemoExitCode::Success)
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
    if (ProductionRuntime)
    {
        ProductionRuntime->DeferredRenderSnapshot.reset();
        ProductionRuntime->ForwardRenderSnapshot.reset();
        ProductionRuntime->LoadedClosure = {};
        ProductionRuntime.reset();
    }
    if (ProductionContentSession)
    {
        (void)ProductionContentSession->Shutdown();
        ProductionContentSession.reset();
    }
    if (ProductionSubmissionHarness) ProductionSubmissionHarness->Release();
    ProductionSubmissionHarness.reset();
    if (BackendRuntime)
    {
        (void)BackendRuntime->Shutdown();
        BackendRuntime.reset();
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
