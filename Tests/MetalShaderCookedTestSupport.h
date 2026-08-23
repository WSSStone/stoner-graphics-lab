#pragma once

#include "Asset/AssetMinimal.h"
#include "AssetCooker/FAssetCookResult.h"
#include "FCookedGenerationPublisher.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>

namespace Stoner::Tests::MetalShaderCooked
{

inline Asset::FAssetDigest Digest(std::string_view Text)
{
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
}

inline Asset::FAssetId Id(
    const char* Type,
    const char* Path,
    const char* Subresource = nullptr)
{
    Asset::FAssetId Value;
    std::optional<Core::FString> Sub;
    if (Subresource) Sub = Core::FString(Subresource);
    (void)Asset::FAssetId::Create(
        Core::FString(Type), Core::FString(Path), Sub, Value);
    return Value;
}

inline Asset::FAssetParticipantId Participant(const char* Text)
{
    Asset::FAssetParticipantId Value;
    (void)Asset::FAssetParticipantId::Create(Core::FString(Text), Value);
    return Value;
}

inline Asset::FAssetProducerVersion Producer(const char* Text)
{
    Asset::FAssetProducerVersion Value;
    (void)Asset::FAssetProducerVersion::Create(Core::FString(Text), Value);
    return Value;
}

inline Core::TArray<Core::uint8> Read(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
}

inline bool ParseProfile(
    const char* Path,
    Asset::FAssetTargetProfileEvidence& Out)
{
    const auto Bytes = Read(Path);
    return Asset::FAssetCookContractCodec::ParseTargetProfile(Bytes, Out) ==
        Asset::EAssetResult::Success;
}

inline Asset::FShaderNativeBindingEvidence BindingEvidence(
    Asset::EShaderStage Stage,
    bool bHasUniformBinding = true)
{
    Asset::FShaderNativeBindingEvidence Evidence;
    Evidence.PolicyVersion = Core::FString("metal-direct-binding-v1");
    if (bHasUniformBinding)
        Evidence.Entries = {{
            Stage, 0, 0, Asset::EShaderResourceKind::UniformBuffer,
            0, Asset::EShaderNativeResourceClass::Buffer, 1}};
    Evidence.ReservedRanges = {{
        Stage, Asset::EShaderNativeResourceClass::Buffer,
        0, 1, Core::FString("engine-constant-data")}};
    Evidence.LimitSnapshot = {
        {Stage, Asset::EShaderNativeResourceClass::Buffer, 31},
        {Stage, Asset::EShaderNativeResourceClass::Texture, 128},
        {Stage, Asset::EShaderNativeResourceClass::Sampler, 16}};
    std::sort(Evidence.LimitSnapshot.begin(), Evidence.LimitSnapshot.end(),
        [](const auto& Left, const auto& Right)
        {
            return std::tuple(Left.Stage, Left.NativeClass) <
                std::tuple(Right.Stage, Right.NativeClass);
        });
    (void)Asset::FinalizeShaderNativeBindingEvidence(Evidence);
    return Evidence;
}

inline Asset::FShaderNativeLibraryEvidence LibraryEvidence(
    const Core::TArray<Core::uint8>& Bytes,
    const Core::FString& Profile,
    const Core::FString& Architecture)
{
    Asset::FShaderNativeLibraryEvidence Evidence;
    Evidence.DerivationEvidenceDigest = Digest("derived-metal-library-v1");
    Evidence.TargetProfile = Profile;
    Evidence.Architecture = Architecture;
    Evidence.Compiler = Core::FString("test-metal-compiler");
    Evidence.XcodeBuild = Core::FString("test-xcode-build");
    Evidence.Sdk = Core::FString("test-macos-sdk");
    Evidence.DeploymentTarget = Core::FString("12.0");
    Evidence.LanguageVersion = Core::FString("2.4");
    Evidence.ArgumentDigest = Digest("test-metal-argv");
    Evidence.LibraryDigest = Asset::FAssetDigest::FromBytes(Bytes);
    Evidence.SizeBytes = Bytes.size();
    Evidence.Finalizer = Participant("cooker.metal-shader");
    Evidence.FinalizerVersion = Producer("027-v2");
    (void)Asset::FinalizeShaderNativeLibraryEvidence(Evidence);
    return Evidence;
}

inline Core::TSharedPtr<const Asset::FShaderPayloadAsset> MetalPayload(
    Asset::EShaderStage Stage,
    const char* Subresource,
    const char* SourceSeed,
    Core::uint8 ByteTag,
    const Core::FString& Profile = Core::FString("metal-macos-12-arm64"),
    const Core::FString& Architecture = Core::FString("arm64"),
    bool bHasUniformBinding = true)
{
    const Core::TArray<Core::uint8> Bytes = {
        'M', 'T', 'L', 'B', ByteTag};
    Asset::FAssetVersion Version;
    Version.SourceDigest = Digest(SourceSeed);
    Version.ContentDigest = Asset::FAssetDigest::FromBytes(Bytes);
    Version.CookDigest = Version.ContentDigest;
    Version.Producer = Participant("cooker.metal-shader");
    Version.ProducerVersion = Producer("027-v2");
    Version.TargetProfile = Profile;
    Asset::FShaderPayloadAsset Payload;
    const auto Created = Asset::FShaderPayloadAsset::CreateWithNativeEvidence(
        Id("ShaderPayload", "Tests/Metal/Strict", Subresource),
        std::move(Version), Asset::EShaderBackendFamily::Metal,
        Profile, Asset::EShaderPayloadFormat::MetalLibrary, Stage,
        Core::FString("main"), {}, Bytes,
        BindingEvidence(Stage, bHasUniformBinding),
        LibraryEvidence(Bytes, Profile, Architecture), Payload);
    return Created == Asset::EAssetResult::Success
        ? Core::MakeShared<const Asset::FShaderPayloadAsset>(std::move(Payload))
        : nullptr;
}

inline Core::TSharedPtr<const Asset::FShaderPayloadAsset> TriangleMetalPayload(
    Asset::EShaderStage Stage,
    const char* Subresource,
    const char* SourceSeed,
    Core::uint8 ByteTag)
{
    const Core::TArray<Core::uint8> Bytes = {'M', 'T', 'L', 'B', ByteTag};
    const Core::FString Profile("metal-macos-12-arm64");
    Asset::FAssetVersion Version;
    Version.SourceDigest = Digest(SourceSeed);
    Version.ContentDigest = Asset::FAssetDigest::FromBytes(Bytes);
    Version.CookDigest = Version.ContentDigest;
    Version.Producer = Participant("cooker.metal-shader");
    Version.ProducerVersion = Producer("027-v2");
    Version.TargetProfile = Profile;
    Asset::FShaderPayloadAsset Payload;
    const auto Created = Asset::FShaderPayloadAsset::CreateWithNativeEvidence(
        Id("ShaderPayload", "Engine/Shaders/Triangle", Subresource),
        std::move(Version), Asset::EShaderBackendFamily::Metal,
        Profile, Asset::EShaderPayloadFormat::MetalLibrary, Stage,
        Core::FString("main"), {}, Bytes, BindingEvidence(Stage, false),
        LibraryEvidence(Bytes, Profile, "arm64"), Payload);
    return Created == Asset::EAssetResult::Success
        ? Core::MakeShared<const Asset::FShaderPayloadAsset>(std::move(Payload))
        : nullptr;
}

struct FGeneration
{
    Asset::FAssetTargetProfileEvidence Profile;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> Payloads;
    Asset::FAssetCookManifest Manifest;
    Core::FString CanonicalManifest;
    Core::FString ImageRoot;
    Core::TArray<AssetCooker::FAssetCookArtifact> Artifacts;
};

inline const Asset::FAssetId& CookedAssetId(
    const Asset::FShaderPayloadAsset& Payload)
{
    return Payload.GetId();
}

inline const Asset::FAssetVersion& CookedAssetVersion(
    const Asset::FShaderPayloadAsset& Payload)
{
    return Payload.GetVersion();
}

inline const Asset::FAssetId& CookedAssetId(const Asset::FShaderAsset& Asset)
{
    return Asset.GetDesc().Id;
}

inline const Asset::FAssetVersion& CookedAssetVersion(
    const Asset::FShaderAsset& Asset)
{
    return Asset.GetDesc().Version;
}

inline Core::TSharedPtr<const Asset::FShaderAsset> TriangleProgram(
    const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>&
        Payloads)
{
    if (Payloads.size() != 2) return nullptr;
    Asset::FShaderAssetDesc Desc;
    Desc.Id = Id("ShaderProgram", "Engine/Shaders/Triangle");
    Desc.Version.SourceDigest = Digest("triangle-program-v1");
    Desc.Version.ContentDigest = Desc.Version.SourceDigest;
    Desc.ProgramKind = Asset::EShaderProgramKind::Graphics;
    Asset::FShaderVariantDefinition Variant;
    Variant.VariantName = Core::FString("default");
    for (const auto& Payload : Payloads)
    {
        if (!Payload) return nullptr;
        const bool bVertex =
            Payload->GetStage() == Asset::EShaderStage::Vertex;
        const char* Subresource = bVertex
            ? "source.vertex" : "source.fragment";
        Asset::FShaderSourceReference Source;
        Source.Stage = Payload->GetStage();
        Source.EntryPoint = Core::FString("main");
        (void)Asset::TSoftAssetRef<Asset::FShaderSourceAsset>::Create(
            Id("ShaderSource", "Engine/Shaders/Triangle", Subresource),
            Source.Source);
        Source.Locator = Core::FString(Subresource);
        Source.ExpectedDigest = Payload->GetVersion().SourceDigest;
        Desc.Stages.push_back(std::move(Source));

        Asset::FShaderPayloadReference Reference;
        Reference.Backend = Asset::EShaderBackendFamily::Vulkan;
        Reference.Profile = Core::FString("vulkan-1.3");
        Reference.Format = Asset::EShaderPayloadFormat::SPIRV;
        Reference.Stage = Payload->GetStage();
        Reference.EntryPoint = Core::FString("main");
        (void)Asset::TSoftAssetRef<Asset::FShaderPayloadAsset>::Create(
            Payload->GetId(), Reference.Payload);
        Reference.Locator = Core::FString(Subresource);
        Reference.ExpectedDigest = Payload->GetVersion().SourceDigest;
        Reference.Producer = Core::FString("shaderc");
        Reference.ProducerVersion = Core::FString("023-v1");
        Variant.Payloads.push_back(std::move(Reference));
    }
    Desc.Variants.push_back(std::move(Variant));
    Asset::FShaderAsset Program;
    return Asset::FShaderAsset::CreateValidated(std::move(Desc), Program) ==
            Asset::EAssetResult::Success
        ? Core::MakeShared<const Asset::FShaderAsset>(std::move(Program))
        : nullptr;
}

inline bool BuildGeneration(
    const std::filesystem::path& Root,
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>> Payloads,
    FGeneration& Out,
    const char* ProfilePath =
        "Config/AssetCooker/Profiles/Mac-Metal-Arm64.json",
    Core::TSharedPtr<const Asset::FShaderAsset> Program = nullptr)
{
    Out = {};
    if (Payloads.empty() || !ParseProfile(ProfilePath, Out.Profile)) return false;
    Out.Payloads = std::move(Payloads);
    std::sort(Out.Payloads.begin(), Out.Payloads.end(),
        [](const auto& Left, const auto& Right)
        { return Left->GetId() < Right->GetId(); });
    auto& Manifest = Out.Manifest;
    Manifest.TargetProfile = Out.Profile;
    Manifest.Selection.Mode = Asset::EAssetCookSelectionMode::ExplicitRoots;
    Manifest.Selection.DiscoveryRulesVersion = Digest("metal-discovery-v1");
    Manifest.Selection.SourceScopes = {Core::FString("scope/0")};
    Manifest.SnapshotDigest = Digest("metal-snapshot-v1");
    Manifest.LimitsDigest = Digest("metal-limits-v1");
    const auto AddPayload = [&](const auto& Payload) -> bool
    {
        const auto& PayloadId = CookedAssetId(*Payload);
        const auto& PayloadVersion = CookedAssetVersion(*Payload);
        Core::TArray<Core::uint8> EnvelopeBytes;
        Asset::FAssetCookedPayloadEnvelope Envelope;
        if (!Payload || Asset::FAssetCookContractCodec::WriteTypedPayload(
                *Payload, {}, EnvelopeBytes, &Envelope) !=
                Asset::EAssetResult::Success)
            return false;
        Asset::FAssetDerivedKey Key;
        if (Asset::FAssetDerivedKey::ParseLowerHex(
                Envelope.EnvelopeDigest.ToLowerHex(), Key) !=
                Asset::EAssetResult::Success)
            return false;
        const std::string Hex =
            Envelope.EnvelopeDigest.ToLowerHex().ToStdString();
        const Core::FString Locator(
            "Payloads/" + Hex.substr(0, 2) + "/" + Hex + ".sgasset");
        Asset::FAssetCookManifestRecord Record;
        Record.AssetId = PayloadId;
        Record.AssetType = Payload->GetAssetType();
        Record.SourceVersion = PayloadVersion.SourceDigest;
        Record.SourceManifest = {{
            Record.AssetId, Record.SourceVersion, Core::FString("primary")}};
        Record.Importer = {
            Participant("stoner.material-shader.dependency"), Producer("023-v1")};
        Record.Cooker = {
            Participant("cooker.metal-shader"), Producer("027-v2")};
        if (Asset::FAssetParticipantId::Create(
                Envelope.Header.CodecId, Record.Codec.Id) !=
                Asset::EAssetResult::Success ||
            Asset::FAssetProducerVersion::Create(
                Core::FString(std::to_string(Envelope.Header.CodecVersion)),
                Record.Codec.Version) != Asset::EAssetResult::Success)
            return false;
        Record.DerivedKey = Key;
        Record.PayloadSchemaVersion = Envelope.Header.PayloadSchemaVersion;
        Record.PayloadLocator = Locator;
        Record.PayloadBytes = EnvelopeBytes.size();
        Record.EnvelopeDigest = Envelope.EnvelopeDigest;
        Manifest.Selection.Roots.push_back(Record.AssetId);
        Manifest.Records.push_back(std::move(Record));
        Out.Artifacts.push_back({
            PayloadId, Locator, Envelope.EnvelopeDigest,
            std::move(EnvelopeBytes)});
        return true;
    };
    for (const auto& Payload : Out.Payloads)
        if (!AddPayload(Payload)) return false;
    if (Program && !AddPayload(Program)) return false;
    std::sort(Manifest.Selection.Roots.begin(), Manifest.Selection.Roots.end());
    std::sort(Manifest.Records.begin(), Manifest.Records.end(),
        [](const auto& Left, const auto& Right)
        { return Left.AssetId < Right.AssetId; });
    std::sort(Out.Artifacts.begin(), Out.Artifacts.end(),
        [](const auto& Left, const auto& Right)
        { return Left.AssetId < Right.AssetId; });
    if (Asset::FAssetCookContractCodec::WriteManifest(
            Manifest, {}, Out.CanonicalManifest) != Asset::EAssetResult::Success)
        return false;
    AssetCooker::Private::FCookedGenerationImageRequest Image;
    Image.ScratchRoot = Core::FString((Root / "Scratch").generic_string());
    Image.Manifest = Manifest;
    Image.CanonicalManifest = Out.CanonicalManifest;
    Image.Artifacts = Out.Artifacts;
    const auto Built =
        AssetCooker::Private::FCookedGenerationPublisher::BuildRequestImage(Image);
    Out.ImageRoot = Built.ImageRoot;
    return Built.Succeeded();
}

inline AssetCooker::Private::FCookedGenerationPublicationRequest
PublicationRequest(
    const FGeneration& Generation,
    const std::filesystem::path& Output)
{
    AssetCooker::Private::FCookedGenerationPublicationRequest Request;
    Request.RequestImageRoot = Generation.ImageRoot;
    Request.OutputRoot = Core::FString(Output.generic_string());
    Request.Manifest = Generation.Manifest;
    Request.CanonicalManifest = Generation.CanonicalManifest;
    Request.RevalidateInputs = [] { return Asset::EAssetResult::Success; };
    return Request;
}

} // namespace Stoner::Tests::MetalShaderCooked
