#include "AssetCookerProductionTextureTests.h"

#include "Asset/AssetMinimal.h"
#include "FAssetCookerSelection.h"

#include <iostream>

namespace
{

using namespace Stoner;

void Record(
    FAssetCookerProductionTextureTestResult& Result,
    bool Passed,
    const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

Asset::FAssetId MakeId(const char* Name)
{
    Asset::FAssetId Id;
    (void)Asset::FAssetId::Create(
        Core::FString("Texture"),
        Core::FString(std::string("Production/") + Name),
        Core::FString("texture"),
        Id);
    return Id;
}

Core::TSharedPtr<const Asset::FTextureAsset> MakeTexture(
    const char* Name,
    Asset::ETextureSemantic Semantic,
    Asset::EImageColorSpace ColorSpace,
    Asset::EImageMipPolicy MipPolicy)
{
    Asset::FImageMip Base;
    if (Asset::FImageMip::Create(
            {4, 4}, Asset::EImageTexelFormat::R8G8B8A8_UNorm,
            Core::TArray<Core::uint8>(4U * 4U * 4U, 127U), Base) !=
        Asset::EAssetResult::Success)
        return {};
    Asset::FAssetId ImageId;
    (void)Asset::FAssetId::Create(
        Core::FString("Image"),
        Core::FString(std::string("Production/") + Name),
        Core::FString("image"),
        ImageId);
    Asset::FAssetSourceLocator Source;
    (void)Asset::FAssetSourceLocator::Create(
        Core::FString("file"),
        Core::FString(std::string("Images/") + Name + ".png"), Source);
    Asset::FImageAsset Image;
    if (Asset::FImageAsset::Create(
            ImageId, Source, Base, ColorSpace, Asset::EImageAlphaMode::Straight,
            Asset::FAssetDigest::FromBytes(Base.GetBytes()), Image) !=
        Asset::EAssetResult::Success)
        return {};

    Asset::FImageImportSettings Settings;
    Settings.Semantic = Semantic;
    Settings.ColorSpace = ColorSpace;
    Settings.MipPolicy = MipPolicy;
    Core::TArray<Asset::FImageMip> Mips{Base};
    if (MipPolicy == Asset::EImageMipPolicy::FullChain)
    {
        Asset::FImageMip Mip;
        (void)Asset::FImageMip::Create(
            {2, 2}, Asset::EImageTexelFormat::R8G8B8A8_UNorm,
            Core::TArray<Core::uint8>(2U * 2U * 4U, 63U), Mip);
        Mips.push_back(std::move(Mip));
        (void)Asset::FImageMip::Create(
            {1, 1}, Asset::EImageTexelFormat::R8G8B8A8_UNorm,
            Core::TArray<Core::uint8>(4U, 31U), Mip);
        Mips.push_back(std::move(Mip));
    }
    Asset::FTextureAsset Texture;
    if (Asset::FTextureAsset::Create(
            MakeId(Name), Core::MakeShared<Asset::FImageAsset>(std::move(Image)),
            Settings, std::move(Mips), Texture) != Asset::EAssetResult::Success)
        return {};
    return Core::MakeShared<Asset::FTextureAsset>(std::move(Texture));
}

Asset::FAssetTargetProfileEvidence Profile()
{
    Core::TArray<Core::uint8> Bytes;
    (void)Core::FPlatformFileSystem::ReadFile(
        Core::FString("Config/AssetCooker/Profiles/Mac-Vulkan.json"), Bytes);
    Asset::FAssetTargetProfileEvidence Result;
    (void)Asset::FAssetCookContractCodec::ParseTargetProfile(Bytes, Result);
    return Result;
}

} // namespace

FAssetCookerProductionTextureTestResult
RunAssetCookerProductionTextureTests()
{
    using namespace Stoner;
    FAssetCookerProductionTextureTestResult Result;
    Asset::FAssetExtensionRegistry Registry;
    Asset::FAssetCookedExtensionRegistrations Generic;
    Asset::FAssetRegistrationToken KtxToken;
    const bool Registered =
        Asset::RegisterCookedAssetExtensions(Registry, Generic) ==
            Asset::EAssetResult::Success &&
        Asset::RegisterKTX2TextureCooker(Registry, KtxToken) ==
            Asset::EAssetResult::Success;
    const auto Target = Profile();

    bool SemanticsPass = Registered;
    Core::TArray<Asset::FAssetDigest> LayoutEvidence;
    for (const auto Semantic : {Asset::ETextureSemantic::Color,
             Asset::ETextureSemantic::Normal, Asset::ETextureSemantic::Data})
    {
        const auto Texture = MakeTexture(
            Semantic == Asset::ETextureSemantic::Color ? "Color" :
            Semantic == Asset::ETextureSemantic::Normal ? "Normal" : "Data",
            Semantic,
            Semantic == Asset::ETextureSemantic::Color
                ? Asset::EImageColorSpace::SRGB
                : Asset::EImageColorSpace::Linear,
            Asset::EImageMipPolicy::FullChain);
        AssetCooker::Private::FAssetCookerSelection Selection;
        const auto Selected = Texture
            ? AssetCooker::Private::SelectAssetCooker(
                  Asset::EAssetCookedFamily::ImageTexture,
                  *Texture, Target, Registry, Selection)
            : Asset::EAssetResult::InvalidInput;
        const auto Parameters = std::dynamic_pointer_cast<
            const Asset::FTextureCookParameters>(Selection.Parameters);
        SemanticsPass = SemanticsPass &&
            Selected == Asset::EAssetResult::Success && Parameters &&
            Selection.CookerId.ToString() == Core::FString("cooker.ktx2") &&
            Selection.OutputContract.CodecId == Core::FString("stoner.ktx2") &&
            Parameters->TextureId == Texture->GetId() &&
            Parameters->Limits.MaxTargetPayloadBytes ==
                Target.Profile.Limits.MaxPayloadBytes &&
            Selection.AdditionalEvidence.size() == 2;
        if (Selection.AdditionalEvidence.size() == 2)
            LayoutEvidence.push_back(Selection.AdditionalEvidence[1].Digest);
    }
    Record(Result,
        SemanticsPass && LayoutEvidence.size() == 3 &&
            LayoutEvidence[0] != LayoutEvidence[1] &&
            LayoutEvidence[1] != LayoutEvidence[2],
        "color normal and data textures select KTX2 with distinct semantic evidence");

    const auto BaseOnly = MakeTexture(
        "MipEvidence", Asset::ETextureSemantic::Color,
        Asset::EImageColorSpace::SRGB, Asset::EImageMipPolicy::BaseOnly);
    const auto FullChain = MakeTexture(
        "MipEvidence", Asset::ETextureSemantic::Color,
        Asset::EImageColorSpace::SRGB, Asset::EImageMipPolicy::FullChain);
    AssetCooker::Private::FAssetCookerSelection BaseSelection;
    AssetCooker::Private::FAssetCookerSelection FullSelection;
    const bool MipEvidencePass = BaseOnly && FullChain &&
        AssetCooker::Private::SelectAssetCooker(
            Asset::EAssetCookedFamily::ImageTexture, *BaseOnly, Target,
            Registry, BaseSelection) == Asset::EAssetResult::Success &&
        AssetCooker::Private::SelectAssetCooker(
            Asset::EAssetCookedFamily::ImageTexture, *FullChain, Target,
            Registry, FullSelection) == Asset::EAssetResult::Success &&
        BaseSelection.AdditionalEvidence.size() == 2 &&
        FullSelection.AdditionalEvidence.size() == 2 &&
        BaseSelection.AdditionalEvidence[1].Digest !=
            FullSelection.AdditionalEvidence[1].Digest;
    Record(Result, MipEvidencePass,
        "mip structure changes selected texture layout evidence");

    auto PortableProfile = Target.Profile;
    PortableProfile.TextureCapabilities = {Core::FString("rgba16-float")};
    PortableProfile.TextureFallback =
        Asset::EAssetTextureFallback::PortableKTX2;
    Core::FString PortableCanonical;
    Asset::FAssetTargetProfileEvidence PortableEvidence;
    const bool PortableWritten =
        Asset::FAssetCookContractCodec::WriteTargetProfile(
            PortableProfile, PortableCanonical, &PortableEvidence) ==
        Asset::EAssetResult::Success;
    AssetCooker::Private::FAssetCookerSelection PortableSelection;
    Record(Result,
        BaseOnly && PortableWritten &&
            AssetCooker::Private::SelectAssetCooker(
                Asset::EAssetCookedFamily::ImageTexture, *BaseOnly,
                PortableEvidence, Registry, PortableSelection) ==
                Asset::EAssetResult::Success &&
            PortableSelection.TargetDecision.Selection ==
                Core::FString("texture-fallback:portable-ktx2") &&
            PortableSelection.TargetDecision.bUsedFallback,
        "portable KTX2 fallback remains an explicit selected decision");

    Core::TArray<Core::uint8> GenericEnvelope;
    Asset::FAssetCookedPayloadEnvelope GenericDetails;
    Asset::FAssetCookResult WrongProducerResult;
    WrongProducerResult.Result = Asset::EAssetResult::Success;
    WrongProducerResult.Payload = BaseOnly;
    if (BaseOnly)
        (void)Asset::FAssetCookContractCodec::WriteTypedPayload(
            *BaseOnly, {}, GenericEnvelope, &GenericDetails);
    WrongProducerResult.Artifact = GenericEnvelope;
    WrongProducerResult.CookDigest = GenericDetails.EnvelopeDigest;
    Core::TArray<Core::uint8> RejectedEnvelope;
    Record(Result,
        AssetCooker::Private::NormalizeCookedArtifact(
            BaseSelection.CookerId, BaseSelection.OutputContract,
            WrongProducerResult, {},
            RejectedEnvelope) == Asset::EAssetResult::TypeMismatch &&
            RejectedEnvelope.empty(),
        "generic texture output cannot impersonate the selected KTX2 producer");

    Asset::FAssetExtensionRegistry MissingKtxRegistry;
    Asset::FAssetCookedExtensionRegistrations MissingKtxGeneric;
    (void)Asset::RegisterCookedAssetExtensions(
        MissingKtxRegistry, MissingKtxGeneric);
    const auto Texture = MakeTexture(
        "NoFallback", Asset::ETextureSemantic::Color,
        Asset::EImageColorSpace::SRGB, Asset::EImageMipPolicy::BaseOnly);
    AssetCooker::Private::FAssetCookerSelection MissingSelection;
    Record(Result,
        Texture && AssetCooker::Private::SelectAssetCooker(
            Asset::EAssetCookedFamily::ImageTexture, *Texture, Target,
            MissingKtxRegistry, MissingSelection) != Asset::EAssetResult::Success &&
            !MissingSelection.CookerId.IsValid(),
        "missing KTX2 producer fails without generic texture fallback");

    return Result;
}
