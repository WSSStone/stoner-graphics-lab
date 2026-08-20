#include "MetalShaderRuntimeTests.h"

#include "Asset/FAssetManager.h"
#include "Asset/FAssetManagerConfig.h"
#include "MetalShaderCookedTestSupport.h"
#include "Renderer/FShaderAssetConversion.h"

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

bool MakeProgram(
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>& Payloads,
    Asset::FShaderAsset& Out)
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
    Desc.InterfaceBindings.push_back({
        0, 0, Asset::EShaderResourceKind::UniformBuffer, 1,
        {Asset::EShaderStage::Vertex, Asset::EShaderStage::Fragment},
        Core::FString("Frame")});
    return Asset::FShaderAsset::CreateValidated(std::move(Desc), Out) ==
        Asset::EAssetResult::Success;
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

} // namespace

FMetalShaderRuntimeTestResult RunMetalShaderRuntimeTests()
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

    std::filesystem::remove_all(Root, Error);
    return Result;
}
