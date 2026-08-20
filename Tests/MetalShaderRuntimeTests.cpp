#include "MetalShaderRuntimeTests.h"

#include "Asset/FAssetManager.h"
#include "Asset/FAssetManagerConfig.h"
#include "Asset/FAssetCookContractCodec.h"
#include "Core/FPlatformFileSystem.h"
#include "Core/SGPlatform.h"
#include "MetalShaderCookedTestSupport.h"
#include "Renderer/FShaderAssetConversion.h"
#include "RHI/FRHIComputePipelineDesc.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"
#include "RHI/FRHIPipelineLayoutDesc.h"

#if SG_PLATFORM_MAC
#include "MetalRHI/FMetalDeviceFactory.h"
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Tests::MetalShaderCooked;

void Record(
    FMetalShaderRuntimeTestResult& Result,
    bool Passed,
    const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

bool WaitTerminal(
    const Asset::FAssetManager& Manager,
    Asset::FAssetRequestHandle Request,
    Asset::FAssetRequestSnapshot& Out)
{
    using namespace std::chrono_literals;
    for (int Attempt = 0; Attempt < 400; ++Attempt)
    {
        if (Manager.Query(Request, Out) != Asset::EAssetResult::Success)
            return false;
        if (Out.State == Asset::EAssetRequestState::Ready ||
            Out.State == Asset::EAssetRequestState::Failed ||
            Out.State == Asset::EAssetRequestState::Cancelled)
            return true;
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

Asset::FAssetVersion Version(std::string_view Seed)
{
    Asset::FAssetVersion Value;
    Value.SourceDigest = Digest(Seed);
    Value.ContentDigest = Value.SourceDigest;
    return Value;
}

class FLookup final : public Asset::IShaderPayloadLookup
{
public:
    explicit FLookup(
        Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> Values)
        : Values_(std::move(Values))
    {
    }

    Core::TSharedPtr<const Asset::FShaderPayloadAsset> Find(
        const Asset::FAssetId& Identity) const override
    {
        for (const auto& Value : Values_)
            if (Value && Value->GetId() == Identity) return Value;
        return nullptr;
    }

private:
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> Values_;
};

struct FProductionLibraryDigest
{
    Asset::FAssetId Id;
    Asset::FAssetDigest Digest;
};

bool MakeProgram(
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>& Payloads,
    Asset::FShaderAsset& Out,
    bool bCombinedSampler = false)
{
    if (Payloads.size() != 2) return false;
    Asset::FShaderAssetDesc Desc;
    Desc.Id = Id("ShaderProgram", "Tests/Metal/Strict");
    Desc.Version = Version("strict-program-v1");
    Desc.ProgramKind = Asset::EShaderProgramKind::Graphics;
    Asset::FShaderVariantDefinition Variant;
    Variant.VariantName = Core::FString("default");
    for (const auto& Payload : Payloads)
    {
        const auto Stage = Payload->GetStage();
        const char* Suffix = Stage == Asset::EShaderStage::Vertex
            ? "source.vertex" : "source.fragment";
        Asset::FShaderSourceReference Source;
        Source.Stage = Stage;
        Source.EntryPoint = Core::FString("main");
        (void)Asset::TSoftAssetRef<Asset::FShaderSourceAsset>::Create(
            Id("ShaderSource", "Tests/Metal/Strict", Suffix), Source.Source);
        Source.Locator = Core::FString(Suffix);
        Source.ExpectedDigest = Payload->GetVersion().SourceDigest;
        Desc.Stages.push_back(std::move(Source));

        Asset::FShaderPayloadReference Reference;
        Reference.Backend = Asset::EShaderBackendFamily::Vulkan;
        Reference.Profile = Core::FString("vulkan-1.3");
        Reference.Format = Asset::EShaderPayloadFormat::SPIRV;
        Reference.Stage = Stage;
        Reference.EntryPoint = Core::FString("main");
        (void)Asset::TSoftAssetRef<Asset::FShaderPayloadAsset>::Create(
            Payload->GetId(), Reference.Payload);
        Reference.Locator = Core::FString(Suffix);
        Reference.ExpectedDigest = Payload->GetVersion().SourceDigest;
        Reference.Producer = Core::FString("shaderc");
        Reference.ProducerVersion = Core::FString("023-v1");
        Variant.Payloads.push_back(std::move(Reference));
    }
    Desc.Variants.push_back(std::move(Variant));
    Desc.InterfaceBindings.push_back(bCombinedSampler
        ? Asset::FShaderInterfaceBinding{
              2, 0, Asset::EShaderResourceKind::CombinedTextureSampler, 1,
              {Asset::EShaderStage::Fragment}, Core::FString("Texture")}
        : Asset::FShaderInterfaceBinding{
              0, 0, Asset::EShaderResourceKind::UniformBuffer, 1,
              {Asset::EShaderStage::Vertex, Asset::EShaderStage::Fragment},
              Core::FString("Frame")});
    return Asset::FShaderAsset::CreateValidated(std::move(Desc), Out) ==
        Asset::EAssetResult::Success;
}

Core::TSharedPtr<const Asset::FShaderPayloadAsset> CombinedSamplerPayload(
    const Asset::FShaderPayloadAsset& Source)
{
    const auto Bytes = Source.GetBytes();
    Asset::FShaderNativeBindingEvidence Binding =
        BindingEvidence(Asset::EShaderStage::Fragment, false);
    Binding.Entries = {
        {Asset::EShaderStage::Fragment, 2, 0,
         Asset::EShaderResourceKind::CombinedTextureSampler, 0,
         Asset::EShaderNativeResourceClass::Texture, 0},
        {Asset::EShaderStage::Fragment, 2, 0,
         Asset::EShaderResourceKind::CombinedTextureSampler, 0,
         Asset::EShaderNativeResourceClass::Sampler, 0}};
    (void)Asset::FinalizeShaderNativeBindingEvidence(Binding);
    Asset::FShaderPayloadAsset Payload;
    const auto Created = Asset::FShaderPayloadAsset::CreateWithNativeEvidence(
        Source.GetId(), Source.GetVersion(), Source.GetBackend(),
        Source.GetProfile(), Source.GetFormat(), Source.GetStage(),
        Source.GetEntryPoint(), Source.GetPermutation(), Bytes,
        std::move(Binding),
        LibraryEvidence(Bytes, Source.GetProfile(), "arm64"), Payload);
    return Created == Asset::EAssetResult::Success
        ? Core::MakeShared<const Asset::FShaderPayloadAsset>(std::move(Payload))
        : nullptr;
}

Core::TSharedPtr<const Asset::FShaderPayloadAsset> MutatedPayload(
    const Asset::FShaderPayloadAsset& Source,
    Asset::EShaderBackendFamily Backend,
    const Core::FString& Profile,
    Asset::EShaderStage Stage,
    const Core::FString& Entry,
    std::string_view SourceVersion,
    Core::uint32 LogicalBinding)
{
    const auto Bytes = Source.GetBytes();
    Asset::FAssetVersion PayloadVersion = Source.GetVersion();
    PayloadVersion.SourceDigest = Digest(SourceVersion);
    PayloadVersion.ContentDigest = Asset::FAssetDigest::FromBytes(Bytes);
    PayloadVersion.CookDigest = PayloadVersion.ContentDigest;
    PayloadVersion.TargetProfile = Profile;
    auto Binding = BindingEvidence(Stage);
    Binding.Entries.front().BindingIndex = LogicalBinding;
    (void)Asset::FinalizeShaderNativeBindingEvidence(Binding);
    Asset::FShaderPayloadAsset Value;
    if (Backend == Asset::EShaderBackendFamily::Metal)
    {
        const auto Created = Asset::FShaderPayloadAsset::CreateWithNativeEvidence(
            Source.GetId(), std::move(PayloadVersion), Backend, Profile,
            Asset::EShaderPayloadFormat::MetalLibrary, Stage, Entry, {}, Bytes,
            std::move(Binding), LibraryEvidence(Bytes, Profile, "arm64"), Value);
        return Created == Asset::EAssetResult::Success
            ? Core::MakeShared<const Asset::FShaderPayloadAsset>(std::move(Value))
            : nullptr;
    }
    return nullptr;
}

Asset::EAssetResult Select(
    const Asset::FShaderAsset& Program,
    const FLookup& Lookup,
    Asset::FSelectedShaderProgram& Out,
    Asset::EShaderBackendFamily Backend = Asset::EShaderBackendFamily::Metal,
    std::optional<Asset::EAssetTargetCpuArchitecture> Cpu =
        Asset::EAssetTargetCpuArchitecture::Arm64,
    const char* Profile = "metal-macos-12-arm64")
{
    Asset::FShaderTargetRequest Target;
    Target.Backend = Backend;
    Target.CpuArchitecture = Cpu;
    Target.AcceptableProfiles = {Core::FString(Profile)};
    return Asset::SelectShaderProgram(Program, Target, Lookup, Out);
}

bool LoadProductionProgram(
    Asset::FAssetManager& Manager,
    const Asset::FAssetTargetProfileEvidence& TargetEvidence,
    const Core::FString& SelectedProfile,
    const char* ProgramPath,
    Core::usize ExpectedStageCount,
    Renderer::FShaderAssetSnapshot& Out,
    Core::TArray<FProductionLibraryDigest>* OutLibraries = nullptr)
{
    Asset::FAssetId ProgramId;
    if (Asset::FAssetId::Create(
            "ShaderProgram", ProgramPath, std::nullopt, ProgramId) !=
        Asset::EAssetResult::Success)
        return false;

    Core::TArray<Asset::FAssetRequestHandle> Requests;
    const auto Release = [&Manager, &Requests]()
    {
        for (const auto Request : Requests)
            if (Request.IsValid()) (void)Manager.ReleaseRequest(Request);
    };
    Asset::FAssetRequestHandle ProgramRequest;
    Asset::FAssetRequestSnapshot RequestSnapshot;
    Asset::TAssetHandle<Asset::FShaderAsset> Program;
    if (Manager.Request<Asset::FShaderAsset>(ProgramId, ProgramRequest) !=
            Asset::EAssetResult::Success ||
        !WaitTerminal(Manager, ProgramRequest, RequestSnapshot) ||
        RequestSnapshot.State != Asset::EAssetRequestState::Ready ||
        Manager.GetResult(ProgramRequest, Program) !=
            Asset::EAssetResult::Success || !Program.IsValid())
    {
        Requests.push_back(ProgramRequest);
        Release();
        return false;
    }
    Requests.push_back(ProgramRequest);

    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> Payloads;
    for (const auto& Variant : Program->GetDesc().Variants)
    {
        if (!Variant.Permutation.Flags.empty()) continue;
        for (const auto& Reference : Variant.Payloads)
        {
            const bool bDerivedMetal =
                Reference.Backend == Asset::EShaderBackendFamily::Vulkan &&
                Reference.Format == Asset::EShaderPayloadFormat::SPIRV;
            const bool bExactMetal =
                Reference.Backend == Asset::EShaderBackendFamily::Metal &&
                Reference.Format == Asset::EShaderPayloadFormat::MetalLibrary;
            const auto& PayloadId = Reference.Payload.GetId();
            if ((!bDerivedMetal && !bExactMetal) || !PayloadId) continue;
            Asset::FAssetRequestHandle PayloadRequest;
            Asset::TAssetHandle<Asset::FShaderPayloadAsset> Payload;
            if (Manager.Request<Asset::FShaderPayloadAsset>(
                    *PayloadId, PayloadRequest) != Asset::EAssetResult::Success ||
                !WaitTerminal(Manager, PayloadRequest, RequestSnapshot) ||
                RequestSnapshot.State != Asset::EAssetRequestState::Ready ||
                Manager.GetResult(PayloadRequest, Payload) !=
                    Asset::EAssetResult::Success || !Payload.IsValid())
            {
                Requests.push_back(PayloadRequest);
                Release();
                return false;
            }
            Requests.push_back(PayloadRequest);
            Payloads.push_back(
                Core::MakeShared<const Asset::FShaderPayloadAsset>(*Payload));
        }
        break;
    }

    Asset::FShaderTargetRequest Target;
    Target.Backend = Asset::EShaderBackendFamily::Metal;
    Target.CpuArchitecture = TargetEvidence.Profile.CpuArchitecture;
    Target.AcceptableProfiles = {SelectedProfile};
    FLookup Lookup(Payloads);
    Asset::FSelectedShaderProgram Selected;
    const auto Selection = Payloads.size() == ExpectedStageCount
        ? Asset::SelectShaderProgram(*Program, Target, Lookup, Selected)
        : Asset::EAssetResult::DependencyMismatch;
    const auto Conversion = Selection == Asset::EAssetResult::Success
        ? Renderer::ConvertShaderAsset({&Selected}, Out)
        : Renderer::EMaterialResult::ValidationFailed;
    const bool bLoaded = Payloads.size() == ExpectedStageCount &&
        Selection == Asset::EAssetResult::Success &&
        Conversion == Renderer::EMaterialResult::Success &&
        Out.ModuleDescriptions.size() == ExpectedStageCount;
    if (bLoaded && OutLibraries)
        for (const auto& Payload : Payloads)
            OutLibraries->push_back({
                Payload->GetId(),
                Asset::FAssetDigest::FromBytes(Payload->GetBytes())});
    if (!bLoaded)
    {
        std::cout << "[INFO] production-program path=" << ProgramPath
                  << " payloads=" << Payloads.size()
                  << " selection=" << static_cast<int>(Selection)
                  << " conversion=" << static_cast<int>(Conversion) << '\n';
        for (const auto& Reference : Program->GetDesc().Variants.front().Payloads)
        {
            const auto& ReferenceId = Reference.Payload.GetId();
            const auto Payload = ReferenceId ? Lookup.Find(*ReferenceId) : nullptr;
            Core::usize ExpectedBindings = 0;
            for (const auto& Binding : Program->GetDesc().InterfaceBindings)
                if (std::find(
                        Binding.Visibility.begin(), Binding.Visibility.end(),
                        Reference.Stage) != Binding.Visibility.end())
                    ExpectedBindings += Binding.ArrayCount;
            const auto* Evidence = Payload
                ? Payload->GetNativeBindingEvidence() : nullptr;
            std::cout << "[INFO] production-stage stage="
                      << static_cast<int>(Reference.Stage)
                      << " payload=" << static_cast<bool>(Payload)
                      << " source="
                      << (Payload && Payload->GetVersion().SourceDigest ==
                              Reference.ExpectedDigest)
                      << " binding-expected=" << ExpectedBindings
                      << " binding-actual="
                      << (Evidence ? Evidence->Entries.size() : 0) << '\n';
        }
    }
    Program.Reset();
    Payloads.clear();
    Selected.Stages.clear();
    Release();
    return bLoaded;
}

void TestProductionCookedPipelines(
    FMetalShaderRuntimeTestResult& Result,
    const FMetalTestOptions& Options)
{
    const bool bAnyPath = !Options.CookedPublicationRoot.empty() ||
        !Options.LeaseCoordinationRoot.empty() ||
        !Options.TargetProfilePath.empty();
    if (!bAnyPath) return;
    const bool bComplete = !Options.CookedPublicationRoot.empty() &&
        !Options.LeaseCoordinationRoot.empty() &&
        !Options.TargetProfilePath.empty();
#if SG_PLATFORM_MAC
    Core::TArray<Core::uint8> ProfileBytes;
    Asset::FAssetTargetProfileEvidence TargetEvidence;
    const bool bProfile = bComplete &&
        Core::FPlatformFileSystem::ReadFile(
            Options.TargetProfilePath.c_str(), ProfileBytes) &&
        Asset::FAssetCookContractCodec::ParseTargetProfile(
            ProfileBytes, TargetEvidence) == Asset::EAssetResult::Success &&
        TargetEvidence.Profile.Platform == Asset::EAssetTargetPlatform::MacOS &&
        TargetEvidence.Profile.GraphicsBackend ==
            Asset::EAssetGraphicsBackend::Metal;
    Core::FString SelectedProfile;
    if (bProfile)
    {
        for (const auto& Choice : TargetEvidence.Profile.ShaderPayloadChoices)
            if (Choice.Backend == Asset::EAssetGraphicsBackend::Metal &&
                Choice.Format == Asset::EAssetShaderPayloadFormat::MetalLibrary)
            {
                SelectedProfile = Choice.Profile;
                break;
            }
    }
    std::error_code Error;
    std::filesystem::create_directories(
        Options.LeaseCoordinationRoot, Error);
    Asset::FAssetManagerConfig Config;
    Config.Mode = Asset::EAssetManagerMode::StrictCooked;
    Config.ExtensionRegistry = Core::MakeShared<Asset::FAssetExtensionRegistry>();
    Config.PublicationRoot = Core::FString(Options.CookedPublicationRoot);
    Config.LeaseCoordinationRoot = Core::FString(Options.LeaseCoordinationRoot);
    Config.TargetEvidence =
        Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(TargetEvidence);
    Core::TSharedPtr<Asset::FAssetManager> Manager;
    Asset::FAssetDiagnosticList Diagnostics;
    const bool bManager = bProfile && !SelectedProfile.IsEmpty() && !Error &&
        Asset::FAssetManager::Create(Config, Manager, Diagnostics) ==
            Asset::EAssetResult::Success && Manager;
    Renderer::FShaderAssetSnapshot Graphics;
    Renderer::FShaderAssetSnapshot Compute;
    Core::TArray<Renderer::FShaderAssetSnapshot> DeferredPrograms(5);
    Core::TArray<FProductionLibraryDigest> Libraries;
    const bool bTriangle = bManager && LoadProductionProgram(
            *Manager, TargetEvidence, SelectedProfile,
            "Engine/Shaders/Triangle", 2, Graphics, &Libraries);
    const bool bSurface = bManager && LoadProductionProgram(
            *Manager, TargetEvidence, SelectedProfile,
            "Engine/Shaders/Deferred/Surface", 2, DeferredPrograms[0],
            &Libraries);
    const bool bComposition = bManager && LoadProductionProgram(
            *Manager, TargetEvidence, SelectedProfile,
            "Engine/Shaders/Deferred/Composition", 2, DeferredPrograms[1],
            &Libraries);
    const bool bDirectional = bManager && LoadProductionProgram(
            *Manager, TargetEvidence, SelectedProfile,
            "Engine/Shaders/Deferred/DirectionalLight", 2,
            DeferredPrograms[2], &Libraries);
    const bool bPoint = bManager && LoadProductionProgram(
            *Manager, TargetEvidence, SelectedProfile,
            "Engine/Shaders/Deferred/PointLight", 2, DeferredPrograms[3],
            &Libraries);
    const bool bSpot = bManager && LoadProductionProgram(
            *Manager, TargetEvidence, SelectedProfile,
            "Engine/Shaders/Deferred/SpotLight", 2, DeferredPrograms[4],
            &Libraries);
    const bool bCompute = bManager && LoadProductionProgram(
            *Manager, TargetEvidence, SelectedProfile,
            "Engine/Shaders/Validation/NoOp", 1, Compute, &Libraries);
    std::sort(
        Libraries.begin(), Libraries.end(),
        [](const auto& Left, const auto& Right) { return Left.Id < Right.Id; });
    Libraries.erase(
        std::unique(
            Libraries.begin(), Libraries.end(),
            [](const auto& Left, const auto& Right)
            { return Left.Id == Right.Id; }),
        Libraries.end());
    const bool bStrict = bTriangle && bSurface && bComposition &&
        bDirectional && bPoint && bSpot && bCompute && Libraries.size() == 12;
    if (!bStrict)
    {
        std::cout << "[INFO] production-load triangle=" << bTriangle
                  << " surface=" << bSurface
                  << " composition=" << bComposition
                  << " directional=" << bDirectional
                  << " point=" << bPoint
                  << " spot=" << bSpot
                  << " compute=" << bCompute << '\n';
    }
    if (Manager)
    {
        (void)Manager->Shutdown();
        Manager.reset();
    }

    auto Created = Backend::Metal::CreateMetalDevice();
    Core::TArray<Core::TSharedPtr<RHI::IRHIShaderModule>> GraphicsModules;
    Core::TArray<Core::TSharedPtr<RHI::IRHIShaderModule>> ComputeModules;
    bool bModules = bStrict && Created.Succeeded();
    if (bModules)
    {
        for (const auto& Desc : Graphics.ModuleDescriptions)
        {
            auto Module = Created.Device->CreateShaderModule(Desc);
            bModules = bModules && Module.Succeeded();
            GraphicsModules.push_back(std::move(Module.Object));
        }
        for (const auto& Desc : Compute.ModuleDescriptions)
        {
            auto Module = Created.Device->CreateShaderModule(Desc);
            bModules = bModules && Module.Succeeded();
            ComputeModules.push_back(std::move(Module.Object));
        }
    }
    RHI::FRHIPipelineLayoutDesc GraphicsLayoutDesc;
    GraphicsLayoutDesc.Bindings.push_back({
        0, 0, RHI::ERHIDescriptorType::UniformBuffer, 1,
        RHI::ERHIShaderStageFlags::Vertex |
            RHI::ERHIShaderStageFlags::Fragment});
    RHI::FRHIPipelineLayoutDesc ComputeLayoutDesc;
    ComputeLayoutDesc.Bindings.push_back({
        0, 0, RHI::ERHIDescriptorType::UniformBuffer, 1,
        RHI::ERHIShaderStageFlags::Compute});
    const auto GraphicsLayout = bModules
        ? Created.Device->CreatePipelineLayout(GraphicsLayoutDesc)
        : RHI::TRHIObjectResult<RHI::IRHIPipelineLayout>{};
    const auto ComputeLayout = bModules
        ? Created.Device->CreatePipelineLayout(ComputeLayoutDesc)
        : RHI::TRHIObjectResult<RHI::IRHIPipelineLayout>{};
    RHI::FRHIGraphicsPipelineDesc GraphicsDesc;
    GraphicsDesc.ShaderModules = GraphicsModules;
    GraphicsDesc.PipelineLayout = GraphicsLayout.Object;
    GraphicsDesc.VertexInput.Stride = sizeof(float) * 5;
    GraphicsDesc.VertexInput.Attributes = {
        {0, RHI::ERHIFormat::R32G32_Float, 0},
        {1, RHI::ERHIFormat::R32G32B32_Float, sizeof(float) * 2}};
    GraphicsDesc.Rasterizer.CullMode = RHI::ERHICullMode::None;
    GraphicsDesc.RenderTargets.ColorFormats = {RHI::ERHIFormat::B8G8R8A8_UNorm};
    GraphicsDesc.RuntimeMode = RHI::ERHIRuntimeObjectMode::RealRuntime;
    RHI::FRHIComputePipelineDesc ComputeDesc;
    ComputeDesc.ShaderModules = ComputeModules;
    ComputeDesc.PipelineLayout = ComputeLayout.Object;
    ComputeDesc.RuntimeMode = RHI::ERHIRuntimeObjectMode::RealRuntime;
    const auto GraphicsPipeline = GraphicsLayout.Succeeded()
        ? Created.Device->CreateGraphicsPipeline(GraphicsDesc)
        : RHI::TRHIObjectResult<RHI::IRHIGraphicsPipeline>{};
    const auto ComputePipeline = ComputeLayout.Succeeded()
        ? Created.Device->CreateComputePipeline(ComputeDesc)
        : RHI::TRHIObjectResult<RHI::IRHIComputePipeline>{};
    const bool bPassed = bStrict && bModules && GraphicsPipeline.Succeeded() &&
        ComputePipeline.Succeeded();
    if (!bPassed)
    {
        std::cout << "[INFO] production-pipeline strict=" << bStrict
                  << " modules=" << bModules
                  << " graphics=" << GraphicsPipeline.Succeeded()
                  << " compute=" << ComputePipeline.Succeeded() << '\n';
    }
    if (bPassed)
    {
        std::cout << "[EVIDENCE] metal-production-cooked graphics="
                  << Graphics.ModuleDescriptions.size()
                  << " compute=" << Compute.ModuleDescriptions.size()
                  << " libraries=12";
        for (const auto& Library : Libraries)
            std::cout << " digest="
                      << Library.Digest.ToLowerHex().CStr();
        std::cout << '\n';
    }
    ComputeModules.clear();
    GraphicsModules.clear();
    if (Created.Device) (void)Created.Device->Shutdown();
    Record(Result, bPassed,
        "production generation strictly loads all libraries into native graphics and compute pipelines");
#else
    Record(Result, !bComplete,
        "production Metal pipeline validation is macOS-only");
#endif
}

} // namespace

FMetalShaderRuntimeTestResult RunMetalShaderRuntimeTests(
    const FMetalTestOptions& Options)
{
    using namespace Stoner;
    using namespace Stoner::Tests::MetalShaderCooked;
    namespace Cooker = AssetCooker::Private;

    FMetalShaderRuntimeTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-metal-shader-runtime";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    const auto Payloads = Core::TArray<
        Core::TSharedPtr<const Asset::FShaderPayloadAsset>>{
        MetalPayload(Asset::EShaderStage::Vertex, "payload.vulkan.vertex",
            "source-vertex-v1", 1),
        MetalPayload(Asset::EShaderStage::Fragment, "payload.vulkan.fragment",
            "source-fragment-v1", 2)};
    FGeneration Generation;
    const bool Built = BuildGeneration(Root / "Generation", Payloads, Generation);
    const auto PublicationRoot = Root / "Published";
    const auto Published = Built
        ? Cooker::FCookedGenerationPublisher::Publish(
              PublicationRequest(Generation, PublicationRoot))
        : Cooker::FCookedGenerationPublicationResult{};
    const auto Coordination = Root / "Coordination";
    std::filesystem::create_directories(Coordination, Error);

    Asset::FAssetManagerConfig Config;
    Config.Mode = Asset::EAssetManagerMode::StrictCooked;
    Config.ExtensionRegistry = Core::MakeShared<Asset::FAssetExtensionRegistry>();
    Config.PublicationRoot = Core::FString(PublicationRoot.generic_string());
    Config.LeaseCoordinationRoot = Core::FString(Coordination.generic_string());
    Config.TargetEvidence =
        Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(
            Generation.Profile);
    Core::TSharedPtr<Asset::FAssetManager> Manager;
    Asset::FAssetDiagnosticList Diagnostics;
    const auto Created = Published.Succeeded()
        ? Asset::FAssetManager::Create(Config, Manager, Diagnostics)
        : Asset::EAssetResult::ProcessingFailure;

    Core::TArray<Asset::FAssetRequestHandle> Requests;
    Core::TArray<Asset::TAssetHandle<Asset::FShaderPayloadAsset>> Handles;
    bool StrictLoaded = Created == Asset::EAssetResult::Success && Manager;
    for (const auto& Payload : Payloads)
    {
        Asset::FAssetRequestHandle Request;
        Asset::FAssetRequestSnapshot Snapshot;
        Asset::TAssetHandle<Asset::FShaderPayloadAsset> Handle;
        StrictLoaded = StrictLoaded &&
            Manager->Request<Asset::FShaderPayloadAsset>(
                Payload->GetId(), Request) == Asset::EAssetResult::Success &&
            WaitTerminal(*Manager, Request, Snapshot) &&
            Snapshot.State == Asset::EAssetRequestState::Ready &&
            Manager->GetResult(Request, Handle) == Asset::EAssetResult::Success &&
            Handle.IsValid() &&
            Handle->GetFormat() == Asset::EShaderPayloadFormat::MetalLibrary;
        Requests.push_back(Request);
        Handles.push_back(std::move(Handle));
    }
    Record(Result,
        StrictLoaded && Config.SourceRoot.IsEmpty() &&
            Config.ExtensionRegistry->Snapshot(
                Asset::EAssetExtensionKind::Resolver).empty() &&
            Config.ExtensionRegistry->Snapshot(
                Asset::EAssetExtensionKind::Loader).empty(),
        "strict manager loads v2 Metal libraries with zero source fallback path");

    Asset::FShaderAsset Program;
    const bool ProgramReady = MakeProgram(Payloads, Program);
    FLookup Lookup(Payloads);
    Asset::FSelectedShaderProgram Selected;
    const bool SelectedStrict = ProgramReady &&
        Select(Program, Lookup, Selected) == Asset::EAssetResult::Success &&
        Selected.Stages.size() == 2;

    Asset::FSelectedShaderProgram Rejected;
    const bool TargetRejections = ProgramReady &&
        Select(Program, Lookup, Rejected,
            Asset::EShaderBackendFamily::Metal, std::nullopt) ==
            Asset::EAssetResult::InvalidInput &&
        Select(Program, Lookup, Rejected,
            Asset::EShaderBackendFamily::Metal,
            Asset::EAssetTargetCpuArchitecture::X86_64) !=
            Asset::EAssetResult::Success &&
        Select(Program, Lookup, Rejected,
            Asset::EShaderBackendFamily::Metal,
            Asset::EAssetTargetCpuArchitecture::Arm64,
            "metal-macos-12-other") == Asset::EAssetResult::TargetUnavailable &&
        Select(Program, Lookup, Rejected,
            Asset::EShaderBackendFamily::Vulkan, std::nullopt,
            "vulkan-1.3") != Asset::EAssetResult::Success;
    Record(Result, SelectedStrict && TargetRejections,
        "strict selection rejects missing CPU, wrong CPU, profile, and backend");

    const auto CombinedFragmentSource = MetalPayload(
        Asset::EShaderStage::Fragment, "payload.vulkan.fragment",
        "source-fragment-v1", 2, "metal-macos-12-arm64", "arm64", false);
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>
        CombinedPayloads{
            MetalPayload(
                Asset::EShaderStage::Vertex, "payload.vulkan.vertex",
                "source-vertex-v1", 1, "metal-macos-12-arm64", "arm64", false),
            CombinedFragmentSource
                ? CombinedSamplerPayload(*CombinedFragmentSource) : nullptr};
    Asset::FShaderAsset CombinedProgram;
    Asset::FSelectedShaderProgram CombinedSelected;
    const bool CombinedSelectedStrict =
        CombinedPayloads.front() && CombinedPayloads.back() &&
        MakeProgram(CombinedPayloads, CombinedProgram, true) &&
        Select(
            CombinedProgram, FLookup(CombinedPayloads), CombinedSelected) ==
            Asset::EAssetResult::Success &&
        CombinedSelected.Stages.size() == 2;
    Record(Result, CombinedSelectedStrict,
        "strict Metal selection accepts combined texture/sampler native entry pairs");

    const auto Vertex = Payloads.front();
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> BadStage{
        MutatedPayload(*Vertex, Asset::EShaderBackendFamily::Metal,
            "metal-macos-12-arm64", Asset::EShaderStage::Fragment,
            "main", "source-vertex-v1", 0),
        Payloads.back()};
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> BadEntry{
        MutatedPayload(*Vertex, Asset::EShaderBackendFamily::Metal,
            "metal-macos-12-arm64", Asset::EShaderStage::Vertex,
            "other", "source-vertex-v1", 0),
        Payloads.back()};
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> BadDigest{
        MutatedPayload(*Vertex, Asset::EShaderBackendFamily::Metal,
            "metal-macos-12-arm64", Asset::EShaderStage::Vertex,
            "main", "source-vertex-v2", 0),
        Payloads.back()};
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> BadInterface{
        MutatedPayload(*Vertex, Asset::EShaderBackendFamily::Metal,
            "metal-macos-12-arm64", Asset::EShaderStage::Vertex,
            "main", "source-vertex-v1", 1),
        Payloads.back()};
    const bool PayloadRejections = ProgramReady &&
        Select(Program, FLookup(BadStage), Rejected) != Asset::EAssetResult::Success &&
        Select(Program, FLookup(BadEntry), Rejected) != Asset::EAssetResult::Success &&
        Select(Program, FLookup(BadDigest), Rejected) != Asset::EAssetResult::Success &&
        Select(Program, FLookup(BadInterface), Rejected) != Asset::EAssetResult::Success &&
        Select(Program, FLookup({Payloads.back()}), Rejected) !=
            Asset::EAssetResult::Success;
    Record(Result, PayloadRejections,
        "strict selection rejects stage, entry, version/digest, interface, and missing payload");

    Renderer::FShaderAssetSnapshot Snapshot;
    const auto Converted = SelectedStrict
        ? Renderer::ConvertShaderAsset({&Selected}, Snapshot)
        : Renderer::EMaterialResult::ValidationFailed;
    Selected.Stages.clear();
    for (auto& Handle : Handles) Handle.Reset();
    for (const auto Request : Requests)
        if (Manager) (void)Manager->ReleaseRequest(Request);
    if (Manager) (void)Manager->Shutdown();
    Manager.reset();
    Record(Result,
        Converted == Renderer::EMaterialResult::Success &&
            Snapshot.ModuleDescriptions.size() == 2 &&
            Snapshot.ModuleDescriptions.front().Payload.Bytes ==
                Payloads.front()->GetBytes() &&
            RHI::IsCanonicalRHINativeBindingMap(
                Snapshot.ModuleDescriptions.front().NativeBindingMap),
        "Renderer RHI snapshots own Metal bytes and bindings after Asset release");

    TestProductionCookedPipelines(Result, Options);

    std::filesystem::remove_all(Root, Error);
    return Result;
}
