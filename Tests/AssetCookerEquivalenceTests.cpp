#include "AssetCookerEquivalenceTests.h"

#include "Asset/AssetMinimal.h"
#include "FMaterialShaderJsonCodec.h"
#include "StaticModelTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;
using namespace Stoner::Core;

class FMemorySource final : public IAssetSource
{
public:
    explicit FMemorySource(TArray<uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size()) return EAssetResult::MalformedSource;
        const usize Count = std::min(
            MaximumBytes, Bytes_.size() - static_cast<usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return EAssetResult::Success;
    }

private:
    TArray<uint8> Bytes_;
};

void Record(
    FAssetCookerEquivalenceTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

TArray<uint8> ReadBytes(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
}

FAssetId Id(const char* Type, const char* Path, const char* Subresource = nullptr)
{
    FAssetId Value;
    std::optional<FString> Sub;
    if (Subresource) Sub = FString(Subresource);
    (void)FAssetId::Create(FString(Type), FString(Path), Sub, Value);
    return Value;
}

FAssetSourceLocator Source(const char* Path)
{
    FAssetSourceLocator Value;
    (void)FAssetSourceLocator::Create(
        FString("fixture"), FString(Path), Value);
    return Value;
}

FAssetParticipantId Participant(const char* Name)
{
    FAssetParticipantId Value;
    (void)FAssetParticipantId::Create(FString(Name), Value);
    return Value;
}

FAssetProducerVersion ProducerVersion(const char* Name)
{
    FAssetProducerVersion Value;
    (void)FAssetProducerVersion::Create(FString(Name), Value);
    return Value;
}

FAssetTargetProfileEvidence Profile()
{
    const TArray<uint8> Bytes = ReadBytes(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json");
    FAssetTargetProfileEvidence Evidence;
    (void)FAssetCookContractCodec::ParseTargetProfile(Bytes, Evidence);
    return Evidence;
}

TSharedPtr<const FImageAsset> MakeImage()
{
    FImageMip Mip;
    (void)FImageMip::Create(
        {2, 2}, EImageTexelFormat::R8G8B8A8_UNorm,
        {255, 0, 0, 255, 0, 255, 0, 255,
         0, 0, 255, 255, 255, 255, 255, 255}, Mip);
    FImageAsset Asset;
    (void)FImageAsset::Create(
        Id("Image", "Cooker/Equivalence", "image"),
        Source("Images/equivalence.png"), Mip,
        EImageColorSpace::SRGB, EImageAlphaMode::Straight,
        FAssetDigest::FromBytes(Mip.GetBytes()), Asset);
    return MakeShared<FImageAsset>(std::move(Asset));
}

TSharedPtr<const FTextureAsset> MakeTexture(
    const TSharedPtr<const FImageAsset>& Image)
{
    FImageImportSettings Settings;
    Settings.Semantic = ETextureSemantic::Color;
    Settings.ColorSpace = EImageColorSpace::SRGB;
    Settings.MipPolicy = EImageMipPolicy::BaseOnly;
    FTextureAsset Asset;
    (void)FTextureAsset::Create(
        Id("Texture", "Cooker/Equivalence", "texture"),
        Image, Settings, {Image->GetBaseMip()}, Asset);
    return MakeShared<FTextureAsset>(std::move(Asset));
}

TSharedPtr<const FKTX2TextureArtifact> LoadKTX2()
{
    const TArray<uint8> Bytes = ReadBytes(
        "Tests/Fixtures/KTX2/Golden/uncompressed-rgba8-alpha.ktx2");
    FKTX2TextureInfo Info;
    if (FKTX2TextureCodec::Inspect(Bytes, {}, Info) != EAssetResult::Success)
        return {};
    FKTX2TextureArtifact Artifact;
    if (FKTX2TextureCodec::Open(
            Info.TextureId, Bytes, {}, Artifact) != EAssetResult::Success)
        return {};
    return MakeShared<FKTX2TextureArtifact>(std::move(Artifact));
}

TSharedPtr<const FAssetPayload> LoadDefinition(
    const std::filesystem::path& Path)
{
    const TArray<uint8> Bytes = ReadBytes(Path);
    FMaterialShaderDefinition Definition;
    if (ParseMaterialShaderDefinition(Bytes, {}, Definition, nullptr) !=
        EAssetResult::Success)
        return {};
    if (auto* Desc = std::get_if<FShaderAssetDesc>(&Definition.Value))
    {
        FShaderAsset Asset;
        if (FShaderAsset::CreateValidated(
                std::move(*Desc), Asset) != EAssetResult::Success)
            return {};
        return MakeShared<FShaderAsset>(std::move(Asset));
    }
    if (auto* Desc = std::get_if<FMaterialAssetDesc>(&Definition.Value))
    {
        FMaterialAsset Asset;
        if (FMaterialAsset::CreateValidated(
                std::move(*Desc), Asset) != EAssetResult::Success)
            return {};
        return MakeShared<FMaterialAsset>(std::move(Asset));
    }
    auto* Desc = std::get_if<FMaterialInstanceAssetDesc>(&Definition.Value);
    FMaterialInstanceAsset Asset;
    if (!Desc || FMaterialInstanceAsset::CreateValidated(
            std::move(*Desc), Asset) != EAssetResult::Success)
        return {};
    return MakeShared<FMaterialInstanceAsset>(std::move(Asset));
}

FAssetVersion ByteVersion(std::span<const uint8> Bytes)
{
    FAssetVersion Version;
    Version.SourceDigest = FAssetDigest::FromBytes(Bytes);
    Version.ContentDigest = Version.SourceDigest;
    return Version;
}

TSharedPtr<const FShaderSourceAsset> MakeShaderSource()
{
    TArray<uint8> Bytes = {'v', 'o', 'i', 'd', ' ', 'm', 'a', 'i', 'n', '(', ')', '{', '}'};
    FShaderSourceAsset Asset;
    (void)FShaderSourceAsset::Create(
        Id("ShaderSource", "Cooker/Equivalence", "source.vertex"),
        ByteVersion(Bytes), EShaderSourceLanguage::GLSL,
        std::move(Bytes), Asset);
    return MakeShared<FShaderSourceAsset>(std::move(Asset));
}

TSharedPtr<const FShaderPayloadAsset> MakeShaderPayload()
{
    TArray<uint8> Bytes = {0x03, 0x02, 0x23, 0x07, 1, 2, 3, 4};
    FShaderPermutationKey Permutation;
    Permutation.Flags = {FString("USE_TEXTURE")};
    FShaderPayloadAsset Asset;
    (void)FShaderPayloadAsset::Create(
        Id("ShaderPayload", "Cooker/Equivalence", "payload.vulkan.vertex"),
        ByteVersion(Bytes), EShaderBackendFamily::Vulkan,
        FString("vulkan-1.3"), EShaderPayloadFormat::SPIRV,
        EShaderStage::Vertex, FString("main"),
        std::move(Permutation), std::move(Bytes), Asset);
    return MakeShared<FShaderPayloadAsset>(std::move(Asset));
}

TArray<TSharedPtr<const FAssetPayload>> LoadStaticPayloads()
{
    EAssetResult Result = EAssetResult::InvalidInput;
    const auto Outputs = StaticModelTestSupport::ImportPackage(
        "Tests/Fixtures/StaticModel/Valid/Geometry/01-basis-u16.gltf",
        Result);
    TArray<TSharedPtr<const FAssetPayload>> Payloads;
    if (Result != EAssetResult::Success) return Payloads;
    for (const auto& Output : Outputs)
    {
        if ((std::dynamic_pointer_cast<const FStaticMeshAsset>(Output.Payload) ||
             std::dynamic_pointer_cast<const FStaticModelAsset>(Output.Payload)) &&
            std::none_of(
                Payloads.begin(), Payloads.end(),
                [&Output](const auto& Existing)
                {
                    return Existing->GetAssetType() ==
                        Output.Payload->GetAssetType();
                }))
            Payloads.push_back(Output.Payload);
    }
    return Payloads;
}

bool KeySemanticsEqual(
    const FAssetPayload& SourcePayload,
    const FAssetPayload& LoadedPayload)
{
    if (SourcePayload.GetAssetType() != LoadedPayload.GetAssetType()) return false;
    if (const auto* SourceImage = dynamic_cast<const FImageAsset*>(&SourcePayload))
    {
        const auto* Loaded = dynamic_cast<const FImageAsset*>(&LoadedPayload);
        return Loaded && Loaded->GetId() == SourceImage->GetId() &&
            Loaded->GetContentDigest() == SourceImage->GetContentDigest();
    }
    if (const auto* SourceTexture = dynamic_cast<const FTextureAsset*>(&SourcePayload))
    {
        const auto* Loaded = dynamic_cast<const FTextureAsset*>(&LoadedPayload);
        return Loaded && Loaded->GetId() == SourceTexture->GetId() &&
            Loaded->GetContentDigest() == SourceTexture->GetContentDigest() &&
            Loaded->GetMips().size() == SourceTexture->GetMips().size();
    }
    if (const auto* SourceKTX =
            dynamic_cast<const FKTX2TextureArtifact*>(&SourcePayload))
    {
        const auto* Loaded =
            dynamic_cast<const FKTX2TextureArtifact*>(&LoadedPayload);
        return Loaded && Loaded->GetInfo() == SourceKTX->GetInfo() &&
            std::equal(
                Loaded->GetBytes().begin(), Loaded->GetBytes().end(),
                SourceKTX->GetBytes().begin(), SourceKTX->GetBytes().end());
    }
    if (const auto* SourceValue =
            dynamic_cast<const FShaderSourceAsset*>(&SourcePayload))
    {
        const auto* Loaded = dynamic_cast<const FShaderSourceAsset*>(&LoadedPayload);
        return Loaded && Loaded->GetId() == SourceValue->GetId() &&
            Loaded->GetVersion() == SourceValue->GetVersion() &&
            Loaded->GetBytes() == SourceValue->GetBytes();
    }
    if (const auto* SourceValue =
            dynamic_cast<const FShaderPayloadAsset*>(&SourcePayload))
    {
        const auto* Loaded = dynamic_cast<const FShaderPayloadAsset*>(&LoadedPayload);
        return Loaded && Loaded->GetId() == SourceValue->GetId() &&
            Loaded->GetVersion() == SourceValue->GetVersion() &&
            Loaded->GetProfile() == SourceValue->GetProfile() &&
            Loaded->GetPermutation() == SourceValue->GetPermutation() &&
            Loaded->GetBytes() == SourceValue->GetBytes();
    }
    if (const auto* SourceValue = dynamic_cast<const FShaderAsset*>(&SourcePayload))
    {
        const auto* Loaded = dynamic_cast<const FShaderAsset*>(&LoadedPayload);
        return Loaded && Loaded->GetDesc().Id == SourceValue->GetDesc().Id &&
            Loaded->GetDesc().CanonicalDefinition ==
                SourceValue->GetDesc().CanonicalDefinition;
    }
    if (const auto* SourceValue = dynamic_cast<const FMaterialAsset*>(&SourcePayload))
    {
        const auto* Loaded = dynamic_cast<const FMaterialAsset*>(&LoadedPayload);
        return Loaded && Loaded->GetDesc().Id == SourceValue->GetDesc().Id &&
            Loaded->GetDesc().CanonicalDefinition ==
                SourceValue->GetDesc().CanonicalDefinition;
    }
    if (const auto* SourceValue =
            dynamic_cast<const FMaterialInstanceAsset*>(&SourcePayload))
    {
        const auto* Loaded =
            dynamic_cast<const FMaterialInstanceAsset*>(&LoadedPayload);
        return Loaded && Loaded->GetDesc().Id == SourceValue->GetDesc().Id &&
            Loaded->GetDesc().CanonicalDefinition ==
                SourceValue->GetDesc().CanonicalDefinition;
    }
    return dynamic_cast<const FStaticMeshAsset*>(&SourcePayload) != nullptr
        ? dynamic_cast<const FStaticMeshAsset*>(&LoadedPayload) != nullptr
        : dynamic_cast<const FStaticModelAsset*>(&SourcePayload) != nullptr &&
              dynamic_cast<const FStaticModelAsset*>(&LoadedPayload) != nullptr;
}

void TestAllFamilies(FAssetCookerEquivalenceTestResult& Result)
{
    const auto Image = MakeImage();
    const auto Texture = MakeTexture(Image);
    TArray<TSharedPtr<const FAssetPayload>> Payloads = {
        Image,
        Texture,
        LoadKTX2(),
        MakeShaderSource(),
        MakeShaderPayload(),
        LoadDefinition("Tests/Fixtures/MaterialShader/Valid/shader-08.json"),
        LoadDefinition("Tests/Fixtures/MaterialShader/Valid/material-00.json"),
        LoadDefinition("Tests/Fixtures/MaterialShader/Valid/instance-00.json")};
    const auto StaticPayloads = LoadStaticPayloads();
    Payloads.insert(Payloads.end(), StaticPayloads.begin(), StaticPayloads.end());

    bool Complete = Payloads.size() == 10 &&
        std::all_of(Payloads.begin(), Payloads.end(), [](const auto& P) { return P != nullptr; });
    bool Equivalent = Complete;
    bool Repeatable = Complete;
    bool MalformedBodiesRejected = Complete;
    for (const auto& Payload : Payloads)
    {
        if (!Payload) continue;
        TArray<uint8> First;
        FAssetCookedPayloadEnvelope FirstEnvelope;
        const EAssetResult Write = FAssetCookContractCodec::WriteTypedPayload(
            *Payload, {}, First, &FirstEnvelope);
        TSharedPtr<const FAssetPayload> Loaded;
        const EAssetResult Load = FAssetCookContractCodec::LoadTypedPayload(
            First, {}, Loaded);
        TArray<uint8> Second;
        const EAssetResult Rewrite = Loaded
            ? FAssetCookContractCodec::WriteTypedPayload(
                  *Loaded, {}, Second)
            : EAssetResult::InvalidInput;
        const bool SemanticPass = Write == EAssetResult::Success &&
            Load == EAssetResult::Success && Loaded &&
            KeySemanticsEqual(*Payload, *Loaded);
        const bool RewritePass = Rewrite == EAssetResult::Success &&
            First == Second && FirstEnvelope.Header.AssetId.IsValid();
        bool MalformedRejected = false;
        if (Write == EAssetResult::Success && FirstEnvelope.Body.size() > 1)
        {
            FirstEnvelope.Body.pop_back();
            TArray<uint8> TruncatedBodyEnvelope;
            const EAssetResult Repack =
                FAssetCookContractCodec::WriteCookedPayload(
                    FirstEnvelope.Header,
                    FirstEnvelope.ReservedHeaderExtensions,
                    FirstEnvelope.Body,
                    {},
                    TruncatedBodyEnvelope);
            TSharedPtr<const FAssetPayload> InvalidPayload;
            const EAssetResult InvalidLoad = Repack == EAssetResult::Success
                ? FAssetCookContractCodec::LoadTypedPayload(
                      TruncatedBodyEnvelope, {}, InvalidPayload)
                : Repack;
            MalformedRejected = Repack == EAssetResult::Success &&
                InvalidLoad != EAssetResult::Success && !InvalidPayload;
        }
        if (!SemanticPass || !RewritePass)
        {
            std::cout << "[DETAIL] codec="
                      << FirstEnvelope.Header.CodecId.ToStdString()
                      << " write=" << static_cast<int>(Write)
                      << " load=" << static_cast<int>(Load)
                      << " rewrite=" << static_cast<int>(Rewrite)
                      << " semantic=" << SemanticPass
                      << " bytes=" << RewritePass << '\n';
        }
        Equivalent = Equivalent && SemanticPass;
        Repeatable = Repeatable && RewritePass;
        MalformedBodiesRejected =
            MalformedBodiesRejected && MalformedRejected;
    }
    Record(Result, Complete, "all ten Feature 021-024 payload families are represented");
    Record(Result, Equivalent, "source models and cooked-loaded models are semantically equivalent");
    Record(Result, Repeatable, "every typed codec rewrites byte-identically");
    Record(
        Result,
        MalformedBodiesRejected,
        "every typed codec rejects a truncated body with a valid envelope digest");
}

FAssetMetadata Metadata(const FImageAsset& Image)
{
    FAssetMetadata Value;
    Value.Id = Image.GetId();
    Value.Source = Image.GetSource();
    Value.Producer = Participant("importer.image");
    Value.ProducerVersion = ProducerVersion("021-v1");
    Value.Version.SourceDigest = Image.GetSourceDigest();
    Value.Version.ContentDigest = Image.GetContentDigest();
    return Value;
}

void TestExtensionDispatch(FAssetCookerEquivalenceTestResult& Result)
{
    FAssetExtensionRegistry Registry;
    FAssetCookedExtensionRegistrations Registrations;
    const EAssetResult Registered = RegisterCookedAssetExtensions(
        Registry, Registrations);
    FAssetCookedExtensionRegistrations Duplicate;
    const EAssetResult DuplicateResult = RegisterCookedAssetExtensions(
        Registry, Duplicate);
    Record(
        Result,
        Registered == EAssetResult::Success && Registrations.IsComplete() &&
            DuplicateResult == EAssetResult::AlreadyExists &&
            !Duplicate.IsComplete(),
        "cooked codec extensions register atomically and reject duplicates");

    const auto Image = MakeImage();
    const auto Evidence = MakeShared<FAssetTargetProfileEvidence>(Profile());
    FAssetCookRequest Request;
    Request.Metadata = Metadata(*Image);
    Request.Payload = Image;
    Request.TargetProfileEvidence = Evidence;
    FAssetParticipantId CookerId;
    (void)GetAssetCookedParticipant(
        EAssetCookedFamily::ImageTexture,
        EAssetExtensionKind::Cooker, CookerId);
    const FAssetCookResult Cooked = FAssetDispatch::Cook(
        Registry, CookerId, Request);

    FAssetTargetProfile MissingProducerProfile = Evidence->Profile;
    MissingProducerProfile.BuildPolicy.ProducerSettings.erase(
        MissingProducerProfile.BuildPolicy.ProducerSettings.begin());
    auto MissingProducerEvidence = MakeShared<FAssetTargetProfileEvidence>();
    FString MissingProducerCanonical;
    (void)FAssetCookContractCodec::WriteTargetProfile(
        MissingProducerProfile,
        MissingProducerCanonical,
        MissingProducerEvidence.get());
    FAssetCookRequest MissingProducerRequest = Request;
    MissingProducerRequest.TargetProfileEvidence = MissingProducerEvidence;
    const FAssetCookResult MissingProducerCook = FAssetDispatch::Cook(
        Registry, CookerId, MissingProducerRequest);

    FAssetLoadRequest LoadRequest;
    LoadRequest.Metadata = Request.Metadata;
    LoadRequest.Source = FAssetSourceLease(
        MakeShared<FMemorySource>(Cooked.Artifact));
    LoadRequest.TargetProfileEvidence = Evidence;
    FAssetParticipantId LoaderId;
    (void)GetAssetCookedParticipant(
        EAssetCookedFamily::ImageTexture,
        EAssetExtensionKind::Loader, LoaderId);
    const FAssetLoadResult Loaded = FAssetDispatch::Load(
        Registry, LoaderId, LoadRequest);
    Record(
        Result,
        Cooked.Result == EAssetResult::Success &&
            Cooked.ProfileProjection.Validate() == EAssetResult::Success &&
            Loaded.Result == EAssetResult::Success && Loaded.Payload &&
            KeySemanticsEqual(*Image, *Loaded.Payload),
        "public extension dispatch preserves typed payload and profile projection evidence");
    Record(
        Result,
        MissingProducerCook.Result == EAssetResult::InvalidInput &&
            MissingProducerCook.Artifact.empty(),
        "cooked extension rejects a profile missing its producer settings declaration");
}

} // namespace

FAssetCookerEquivalenceTestResult RunAssetCookerEquivalenceTests()
{
    FAssetCookerEquivalenceTestResult Result;
    TestAllFamilies(Result);
    TestExtensionDispatch(Result);
    return Result;
}
