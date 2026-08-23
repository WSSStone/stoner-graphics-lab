#include "RendererMaterialShaderAssetTests.h"

#include "Asset/AssetMinimal.h"
#include "Renderer/FMaterialAssetConversion.h"
#include "Renderer/FShaderAssetConversion.h"
#include "Renderer/FShaderLibrary.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <optional>
#include <thread>

namespace
{

using namespace Stoner;

void Record(
    FRendererMaterialShaderAssetTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

Asset::FAssetId Id(
    const char* Type,
    const char* Path,
    const char* Subresource = nullptr)
{
    Asset::FAssetId Value;
    std::optional<Core::FString> SubresourceValue;
    if (Subresource)
    {
        SubresourceValue = Core::FString(Subresource);
    }
    (void)Asset::FAssetId::Create(
        Core::FString(Type),
        Core::FString(Path),
        SubresourceValue,
        Value);
    return Value;
}

Asset::FAssetVersion Version(const char* Text)
{
    const std::string_view View(Text);
    const auto* Bytes =
        reinterpret_cast<const Core::uint8*>(View.data());
    Asset::FAssetVersion Value;
    Value.SourceDigest = Asset::FAssetDigest::FromBytes(
        std::span<const Core::uint8>(Bytes, View.size()));
    Value.ContentDigest = Value.SourceDigest;
    return Value;
}

Core::TArray<Core::uint8> Spirv(Asset::EShaderStage Stage)
{
    const Core::uint32 Model =
        Stage == Asset::EShaderStage::Vertex ? 0U :
        Stage == Asset::EShaderStage::Fragment ? 4U : 5U;
    const Core::uint32 Words[] = {
        0x07230203U,
        0x00010000U,
        0U,
        2U,
        0U,
        (5U << 16U) | 15U,
        Model,
        1U,
        0x6e69616dU,
        0U};
    Core::TArray<Core::uint8> Bytes(sizeof(Words));
    std::memcpy(Bytes.data(), Words, sizeof(Words));
    return Bytes;
}

Core::TSharedPtr<const Asset::FShaderPayloadAsset> Payload(
    Asset::EShaderStage Stage,
    const char* Subresource)
{
    const auto Bytes = Spirv(Stage);
    Asset::FShaderPayloadAsset Value;
    const Asset::FAssetId PayloadId =
        Id("ShaderPayload", "Tests/Shaders/Snapshot", Subresource);
    Asset::FAssetVersion PayloadVersion;
    PayloadVersion.SourceDigest =
        Asset::FAssetDigest::FromBytes(Bytes);
    PayloadVersion.ContentDigest = PayloadVersion.SourceDigest;
    const Asset::EAssetResult Result =
        Asset::FShaderPayloadAsset::Create(
            PayloadId,
            PayloadVersion,
            Asset::EShaderBackendFamily::Vulkan,
            Core::FString("desktop"),
            Asset::EShaderPayloadFormat::SPIRV,
            Stage,
            Core::FString("main"),
            {},
            Bytes,
            Value);
    return Result == Asset::EAssetResult::Success
        ? Core::MakeShared<Asset::FShaderPayloadAsset>(std::move(Value))
        : nullptr;
}

Asset::FShaderNativeBindingEvidence NativeEvidence(
    Asset::EShaderStage Stage)
{
    Asset::FShaderNativeBindingEvidence Evidence;
    Evidence.PolicyVersion = "metal-direct-binding-v1";
    Evidence.ReservedRanges.push_back({
        Stage, Asset::EShaderNativeResourceClass::Buffer,
        0, 1, "constant-data"});
    Evidence.LimitSnapshot = {
        {Stage, Asset::EShaderNativeResourceClass::Buffer, 31},
        {Stage, Asset::EShaderNativeResourceClass::Texture, 128},
        {Stage, Asset::EShaderNativeResourceClass::Sampler, 16}};
    (void)Asset::FinalizeShaderNativeBindingEvidence(Evidence);
    return Evidence;
}

Asset::FShaderNativeLibraryEvidence NativeLibraryEvidence(
    const Core::TArray<Core::uint8>& Bytes)
{
    Asset::FShaderNativeLibraryEvidence Evidence;
    Evidence.DerivationEvidenceDigest = Asset::FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{'d', 'e', 'r', 'i', 'v', 'e'});
    Evidence.TargetProfile = "metal-macos-12-arm64";
    Evidence.Architecture = "arm64";
    Evidence.Compiler = "test-metal-compiler";
    Evidence.XcodeBuild = "test-xcode-build";
    Evidence.Sdk = "test-macos-sdk";
    Evidence.DeploymentTarget = "12.0";
    Evidence.LanguageVersion = "2.4";
    Evidence.ArgumentDigest = Asset::FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{'a', 'r', 'g', 'v'});
    Evidence.LibraryDigest = Asset::FAssetDigest::FromBytes(Bytes);
    Evidence.SizeBytes = Bytes.size();
    (void)Asset::FAssetParticipantId::Create(
        "cooker.metal-shader", Evidence.Finalizer);
    (void)Asset::FAssetProducerVersion::Create(
        "027-v2", Evidence.FinalizerVersion);
    (void)Asset::FinalizeShaderNativeLibraryEvidence(Evidence);
    return Evidence;
}

Core::TSharedPtr<const Asset::FShaderPayloadAsset> MetalPayload(
    Asset::EShaderStage Stage,
    const char* Subresource)
{
    const Core::TArray<Core::uint8> Bytes = {'M', 'T', 'L', 'B'};
    Asset::FShaderPayloadAsset Value;
    const Asset::FAssetId PayloadId =
        Id("ShaderPayload", "Tests/Shaders/MetalSnapshot", Subresource);
    Asset::FAssetVersion PayloadVersion;
    PayloadVersion.SourceDigest = Asset::FAssetDigest::FromBytes(Bytes);
    PayloadVersion.ContentDigest = PayloadVersion.SourceDigest;
    const auto Created =
        Asset::FShaderPayloadAsset::CreateWithNativeEvidence(
            PayloadId, PayloadVersion,
            Asset::EShaderBackendFamily::Metal,
            Core::FString("metal-macos-12-arm64"),
            Asset::EShaderPayloadFormat::MetalLibrary, Stage,
            Core::FString("main"), {}, Bytes, NativeEvidence(Stage),
            NativeLibraryEvidence(Bytes), Value);
    return Created == Asset::EAssetResult::Success
        ? Core::MakeShared<Asset::FShaderPayloadAsset>(std::move(Value))
        : nullptr;
}

Asset::FSelectedShaderProgram SelectedProgram()
{
    Asset::FSelectedShaderProgram Selected;
    Selected.ShaderId =
        Id("ShaderProgram", "Tests/Shaders/Snapshot");
    Selected.ShaderVersion = Version("shader-program");
    Selected.Backend = Asset::EShaderBackendFamily::Vulkan;
    Selected.SelectedProfile = "desktop";
    Selected.Stages = {
        {Asset::EShaderStage::Vertex,
         Payload(Asset::EShaderStage::Vertex, "vertex")},
        {Asset::EShaderStage::Fragment,
         Payload(Asset::EShaderStage::Fragment, "fragment")}};
    Selected.RequiredParameters.push_back(
        {"BaseColor", Asset::EMaterialAssetParameterType::Color});
    Selected.SourceManifest.push_back({
        Selected.ShaderId,
        Selected.ShaderVersion,
        Asset::EAssetSourceRole::Program});
    for (const char* Subresource : {"source.vertex", "source.fragment"})
    {
        Selected.SourceManifest.push_back({
            Id("ShaderSource", "Tests/Shaders/Snapshot", Subresource),
            Version(Subresource),
            Asset::EAssetSourceRole::Source});
    }
    for (const auto& Stage : Selected.Stages)
    {
        Selected.SourceManifest.push_back({
            Stage.Payload->GetId(),
            Stage.Payload->GetVersion(),
            Asset::EAssetSourceRole::Payload});
    }
    (void)Asset::NormalizeSourceManifest(Selected.SourceManifest);
    return Selected;
}

void TestAtomicShaderLibrary(
    FRendererMaterialShaderAssetTestResult& Result)
{
    Renderer::FShaderLibrary Library;
    Renderer::FShaderRecord Existing;
    Existing.ShaderId = "Existing";
    Existing.Variants.push_back({"default", {}, "vertex+fragment"});
    const bool bSeeded =
        Library.RegisterShaderRecord(Existing) ==
        Renderer::EMaterialResult::Success;

    Renderer::FShaderRecord Added;
    Added.ShaderId = "Added";
    Added.Variants.push_back({"default", {}, "vertex+fragment"});
    const Renderer::FShaderRecord Batch[] = {Added, Existing};
    const auto BatchResult = Library.RegisterShaderRecords(Batch);
    Record(
        Result,
        bSeeded &&
            BatchResult == Renderer::EMaterialResult::DuplicateName &&
            Library.FindRecord("Added") == nullptr &&
            Library.FindRecord("Existing") != nullptr &&
            Library.GetRecords().size() == 1,
        "Shader library batch registration is atomic on mid-batch conflict");
}

void TestShaderSnapshot(
    FRendererMaterialShaderAssetTestResult& Result,
    Renderer::FShaderAssetSnapshot& OutSnapshot)
{
    Asset::FSelectedShaderProgram Selected = SelectedProgram();
    Renderer::FMaterialDiagnosticLog Diagnostics;
    const auto Conversion = Renderer::ConvertShaderAsset(
        {&Selected},
        OutSnapshot,
        &Diagnostics);
    if (Conversion != Renderer::EMaterialResult::Success)
    {
        std::cout << Diagnostics.Format().CStr();
    }
    const bool bOwned =
        Conversion == Renderer::EMaterialResult::Success &&
        OutSnapshot.ShaderRecords.size() == 1 &&
        OutSnapshot.ModuleDescriptions.size() == 2 &&
        ReadRHIShaderSpirvWord(
            OutSnapshot.ModuleDescriptions[0].Payload, 0) == 0x07230203U &&
        OutSnapshot.SourceManifest.size() == 5;
    Selected.Stages.clear();
    Record(
        Result,
        bOwned &&
            ReadRHIShaderSpirvWord(
                OutSnapshot.ModuleDescriptions[1].Payload, 0) == 0x07230203U,
        "Shader conversion owns bytecode and complete selected manifest");

    Asset::FSelectedShaderProgram Incomplete = SelectedProgram();
    Incomplete.Stages.pop_back();
    Renderer::FShaderAssetSnapshot Unchanged = OutSnapshot;
    const auto Failure = Renderer::ConvertShaderAsset(
        {&Incomplete},
        Unchanged);
    Record(
        Result,
        Failure != Renderer::EMaterialResult::Success &&
            Unchanged.ModuleDescriptions.size() ==
                OutSnapshot.ModuleDescriptions.size(),
        "Incomplete shader conversion fails without changing destination");

    Asset::FSelectedShaderProgram Conflicting = SelectedProgram();
    Conflicting.SourceManifest.push_back({
        Conflicting.ShaderId,
        Version("different-program"),
        Asset::EAssetSourceRole::Program});
    Renderer::FShaderAssetSnapshot Preserved = OutSnapshot;
    const auto Conflict = Renderer::ConvertShaderAsset(
        {&Conflicting},
        Preserved);
    Record(
        Result,
        Conflict != Renderer::EMaterialResult::Success &&
            Preserved.SourceManifest == OutSnapshot.SourceManifest,
        "Shader conversion rejects conflicting versions without replacing snapshot");

    Core::TArray<Core::uint8> ReaderResults(8, 0);
    Core::TArray<std::thread> Readers;
    const Asset::FSelectedShaderProgram Concurrent = SelectedProgram();
    for (std::size_t Index = 0;
         Index < ReaderResults.size();
         ++Index)
    {
        Readers.emplace_back(
            [Index, &ReaderResults, &Concurrent]()
            {
                Renderer::FShaderAssetSnapshot Snapshot;
                ReaderResults[Index] = static_cast<Core::uint8>(
                    Renderer::ConvertShaderAsset(
                        {&Concurrent},
                        Snapshot) ==
                        Renderer::EMaterialResult::Success &&
                    Snapshot.ModuleDescriptions.size() == 2);
            });
    }
    for (std::thread& Reader : Readers) Reader.join();
    Record(
        Result,
        std::all_of(
            ReaderResults.begin(),
            ReaderResults.end(),
            [](Core::uint8 Value) { return Value != 0; }),
        "Eight concurrent shader snapshot conversions agree");
}

void TestMetalShaderSnapshot(FRendererMaterialShaderAssetTestResult& Result)
{
    Asset::FSelectedShaderProgram Selected = SelectedProgram();
    Selected.Backend = Asset::EShaderBackendFamily::Metal;
    Selected.SelectedProfile = "metal-macos-12-arm64";
    Selected.Stages = {
        {Asset::EShaderStage::Vertex,
         MetalPayload(Asset::EShaderStage::Vertex, "vertex")},
        {Asset::EShaderStage::Fragment,
         MetalPayload(Asset::EShaderStage::Fragment, "fragment")}};
    Selected.SourceManifest.resize(3);
    for (const auto& Stage : Selected.Stages)
    {
        Selected.SourceManifest.push_back({
            Stage.Payload->GetId(), Stage.Payload->GetVersion(),
            Asset::EAssetSourceRole::Payload});
    }
    (void)Asset::NormalizeSourceManifest(Selected.SourceManifest);

    Renderer::FShaderAssetSnapshot Snapshot;
    const auto Converted = Renderer::ConvertShaderAsset(
        {&Selected}, Snapshot);
    const bool bConverted = Converted == Renderer::EMaterialResult::Success &&
        Snapshot.ModuleDescriptions.size() == 2 &&
        Snapshot.ModuleDescriptions[0].Payload.Format ==
            RHI::ERHIShaderPayloadFormat::MetalLibrary &&
        Snapshot.ModuleDescriptions[0].RuntimeMode ==
            RHI::ERHIRuntimeObjectMode::RealRuntime &&
        RHI::IsCanonicalRHINativeBindingMap(
            Snapshot.ModuleDescriptions[0].NativeBindingMap) &&
        Snapshot.ModuleDescriptions[0].NativeBindingMap.CanonicalDigest.Bytes ==
            Selected.Stages[0].Payload->GetNativeBindingEvidence()
                ->CanonicalDigest.GetBytes();
    Selected.Stages.clear();
    Record(Result,
        bConverted &&
            Snapshot.ModuleDescriptions[0].Payload.Bytes ==
                Core::TArray<Core::uint8>({'M', 'T', 'L', 'B'}),
        "Metal shader conversion owns native bytes and authoritative binding evidence");
}

void TestMaterialSnapshot(
    FRendererMaterialShaderAssetTestResult& Result,
    const Renderer::FShaderAssetSnapshot& Shader)
{
    Asset::FResolvedMaterialAsset Resolved;
    Resolved.LeafId =
        Id("MaterialInstance", "Tests/Materials/SnapshotInstance");
    Resolved.LeafVersion = Version("material-instance");
    Resolved.RootMaterialId =
        Id("Material", "Tests/Materials/Snapshot");
    Resolved.RootMaterialVersion = Version("material");
    Resolved.Shader = {};
    (void)Asset::TSoftAssetRef<Asset::FShaderAsset>::Create(
        Id("ShaderProgram", "Tests/Shaders/Snapshot"),
        Resolved.Shader);
    Resolved.EffectiveParameters.push_back({
        "BaseColor",
        Asset::FMaterialAssetParameterValue::FromColor(
            {1.0f, 0.5f, 0.25f, 1.0f})});
    const Asset::FAssetId Texture =
        Id("Texture", "Tests/Textures/Albedo");
    Asset::FMaterialTextureBinding Binding;
    const bool bBindingCreated = Asset::FMaterialTextureBinding::Create(
        Texture,
        1,
        {Asset::EAssetSamplerFilter::Nearest,
         Asset::EAssetSamplerFilter::Linear,
         Asset::EAssetSamplerMipFilter::None,
         Asset::EAssetSamplerAddressMode::MirroredRepeat,
         Asset::EAssetSamplerAddressMode::ClampToEdge},
        Binding) == Asset::EAssetResult::Success;
    Resolved.EffectiveParameters.push_back({
        "Albedo",
        Asset::FMaterialAssetParameterValue::FromTextureBinding(Binding)});
    Resolved.SourceManifest = {
        {Resolved.RootMaterialId,
         Resolved.RootMaterialVersion,
         Asset::EAssetSourceRole::Material},
        {Resolved.LeafId,
         Resolved.LeafVersion,
         Asset::EAssetSourceRole::Parent},
        {Texture,
         Version("texture"),
         Asset::EAssetSourceRole::Texture}};

    Renderer::FMaterialAssetSnapshot Snapshot;
    Renderer::FMaterialDiagnosticLog Diagnostics;
    const auto Conversion = Renderer::ConvertMaterialAsset(
        {&Resolved, &Shader},
        Snapshot,
        &Diagnostics);
    if (Conversion != Renderer::EMaterialResult::Success)
    {
        std::cout << Diagnostics.Format().CStr();
    }
    Record(
        Result,
        bBindingCreated &&
            Conversion == Renderer::EMaterialResult::Success &&
            Snapshot.Material.IsValid() &&
            Snapshot.Material.GetShaderReference() ==
                Id("ShaderProgram", "Tests/Shaders/Snapshot").ToString() &&
            Snapshot.ResourceRequirements.size() == 1 &&
            Snapshot.TextureBindings.size() == 1 &&
            Snapshot.TextureBindings.front().TextureId == Texture &&
            Snapshot.TextureBindings.front().TexCoordSet == 1 &&
            Snapshot.TextureBindings.front().Sampler.MinFilter ==
                RHI::ERHISamplerFilter::Nearest &&
            Snapshot.TextureBindings.front().Sampler.MipFilter ==
                RHI::ERHISamplerMipFilter::None &&
            Snapshot.TextureBindings.front().Sampler.AddressU ==
                RHI::ERHISamplerAddressMode::MirroredRepeat &&
            Snapshot.TextureBindings.front().Sampler.AddressV ==
                RHI::ERHISamplerAddressMode::ClampToEdge &&
            Snapshot.SourceManifest.size() == 8,
        "Resolved material converts to owned Feature 014 snapshot and manifest");

    const Asset::FResolvedMaterialAsset StableResolved = Resolved;
    const std::size_t OwnedParameterCount =
        Snapshot.Material.GetParameters().GetParameters().size();
    Resolved.EffectiveParameters.clear();
    Record(
        Result,
        Snapshot.Material.GetParameters().GetParameters().size() ==
            OwnedParameterCount &&
            Snapshot.Material.IsValid(),
        "Material snapshot owns values after Asset inputs are released");

    Asset::FResolvedMaterialAsset WrongShader = StableResolved;
    (void)Asset::TSoftAssetRef<Asset::FShaderAsset>::Create(
        Id("ShaderProgram", "Tests/Shaders/Other"),
        WrongShader.Shader);
    Renderer::FMaterialAssetSnapshot Preserved = Snapshot;
    Record(
        Result,
        Renderer::ConvertMaterialAsset(
            {&WrongShader, &Shader},
            Preserved) != Renderer::EMaterialResult::Success &&
            Preserved.Material.IsValid(),
        "Material conversion rejects unresolved shader without replacing snapshot");

    Asset::FResolvedMaterialAsset Conflict = StableResolved;
    Conflict.SourceManifest.push_back({
        Conflict.RootMaterialId,
        Version("conflicting-material"),
        Asset::EAssetSourceRole::Material});
    Renderer::FMaterialAssetSnapshot ConflictDestination = Snapshot;
    Record(
        Result,
        Renderer::ConvertMaterialAsset(
            {&Conflict, &Shader},
            ConflictDestination) != Renderer::EMaterialResult::Success &&
            ConflictDestination.SourceManifest ==
                Snapshot.SourceManifest,
        "Material conversion rejects conflicting manifest versions atomically");
}

void TestSamplerIntentConversion(
    FRendererMaterialShaderAssetTestResult& Result)
{
    RHI::FRHISamplerDesc Sampler;
    const Asset::FMaterialSamplerIntent Automatic;
    const bool bAutomatic = Renderer::ConvertMaterialSamplerIntent(
        Automatic,
        Sampler) == Renderer::EMaterialResult::Success &&
        Sampler.MinFilter == RHI::ERHISamplerFilter::Linear &&
        Sampler.MagFilter == RHI::ERHISamplerFilter::Linear &&
        Sampler.MipFilter == RHI::ERHISamplerMipFilter::Linear &&
        Sampler.AddressU == RHI::ERHISamplerAddressMode::Repeat &&
        Sampler.AddressV == RHI::ERHISamplerAddressMode::Repeat;
    const RHI::FRHISamplerDesc Before = Sampler;
    const Asset::FMaterialSamplerIntent Invalid{
        static_cast<Asset::EAssetSamplerFilter>(255),
        Asset::EAssetSamplerFilter::Linear,
        Asset::EAssetSamplerMipFilter::Automatic,
        Asset::EAssetSamplerAddressMode::Repeat,
        Asset::EAssetSamplerAddressMode::Repeat};
    Record(
        Result,
        bAutomatic &&
            Renderer::ConvertMaterialSamplerIntent(Invalid, Sampler) ==
                Renderer::EMaterialResult::ValidationFailed &&
            Sampler.MinFilter == Before.MinFilter &&
            Sampler.MagFilter == Before.MagFilter &&
            Sampler.MipFilter == Before.MipFilter,
        "Asset sampler intent converts deterministically to RHI without mutating output on invalid input");
}

} // namespace

FRendererMaterialShaderAssetTestResult
RunRendererMaterialShaderAssetTests()
{
    FRendererMaterialShaderAssetTestResult Result;
    TestAtomicShaderLibrary(Result);
    Renderer::FShaderAssetSnapshot Shader;
    TestShaderSnapshot(Result, Shader);
    TestMetalShaderSnapshot(Result);
    TestMaterialSnapshot(Result, Shader);
    TestSamplerIntentConversion(Result);
    return Result;
}
