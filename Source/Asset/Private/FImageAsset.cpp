#include "Asset/FImageAsset.h"
#include "Asset/FTextureAsset.h"

#include "FImageValidation.h"

#include <array>
#include <string>

namespace Stoner::Asset
{
namespace
{

void AppendUint32(Core::TArray<Core::uint8>& Bytes, Core::uint32 Value)
{
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes.push_back(static_cast<Core::uint8>(Value >> Shift));
    }
}

void AppendUint64(Core::TArray<Core::uint8>& Bytes, Core::uint64 Value)
{
    for (Core::uint32 Shift = 0; Shift < 64; Shift += 8)
    {
        Bytes.push_back(static_cast<Core::uint8>(Value >> Shift));
    }
}

void AppendString(Core::TArray<Core::uint8>& Bytes, const Core::FString& Value)
{
    AppendUint64(Bytes, Value.Len());
    Bytes.insert(Bytes.end(), Value.View().begin(), Value.View().end());
}

FAssetDigest DigestImage(
    const FAssetId& Id,
    const FImageMip& Mip,
    EImageColorSpace ColorSpace,
    EImageAlphaMode AlphaMode)
{
    Core::TArray<Core::uint8> Bytes;
    AppendString(Bytes, Id.ToString());
    AppendUint32(Bytes, Mip.GetExtent().Width);
    AppendUint32(Bytes, Mip.GetExtent().Height);
    Bytes.push_back(static_cast<Core::uint8>(Mip.GetFormat()));
    Bytes.push_back(static_cast<Core::uint8>(ColorSpace));
    Bytes.push_back(static_cast<Core::uint8>(AlphaMode));
    Bytes.push_back(static_cast<Core::uint8>(EImageOrigin::TopLeft));
    Bytes.insert(Bytes.end(), Mip.GetBytes().begin(), Mip.GetBytes().end());
    return FAssetDigest::FromBytes(Bytes);
}

FAssetDigest DigestTexture(
    const FAssetId& Id,
    const FImageImportSettings& Settings,
    EImageColorSpace ColorSpace,
    const Core::TArray<FImageMip>& Mips)
{
    Core::TArray<Core::uint8> Bytes;
    AppendString(Bytes, Id.ToString());
    Bytes.push_back(static_cast<Core::uint8>(Settings.Semantic));
    Bytes.push_back(static_cast<Core::uint8>(ColorSpace));
    Bytes.push_back(static_cast<Core::uint8>(Settings.MipPolicy));
    Bytes.push_back(static_cast<Core::uint8>(Settings.HDRLayout));
    AppendUint32(Bytes, Settings.Limits.MaxDimension);
    AppendUint64(Bytes, Settings.Limits.MaxSourceBytes);
    AppendUint64(Bytes, Settings.Limits.MaxMipBytes);
    AppendUint64(Bytes, Settings.Limits.MaxDecodedChainBytes);
    AppendUint64(Bytes, Mips.size());
    for (const FImageMip& Mip : Mips)
    {
        AppendUint32(Bytes, Mip.GetExtent().Width);
        AppendUint32(Bytes, Mip.GetExtent().Height);
        Bytes.push_back(static_cast<Core::uint8>(Mip.GetFormat()));
        Bytes.insert(Bytes.end(), Mip.GetBytes().begin(), Mip.GetBytes().end());
    }
    return FAssetDigest::FromBytes(Bytes);
}

} // namespace

EAssetResult FImageAsset::Create(
    FAssetId Id,
    FAssetSourceLocator Source,
    FImageMip BaseMip,
    EImageColorSpace ColorSpace,
    EImageAlphaMode AlphaMode,
    FAssetDigest SourceDigest,
    FImageAsset& OutAsset)
{
    OutAsset = {};
    if (!Id.IsValid() ||
        Id.GetAssetType() != TAssetTypeTraits<FImageAsset>::GetAssetType() ||
        !Source.IsValid() ||
        !BaseMip.IsValid() ||
        !SourceDigest.IsAvailable() ||
        (ColorSpace != EImageColorSpace::Linear &&
         ColorSpace != EImageColorSpace::SRGB) ||
        (AlphaMode != EImageAlphaMode::None &&
         AlphaMode != EImageAlphaMode::Straight))
    {
        return EAssetResult::InvalidInput;
    }
    if (AlphaMode == EImageAlphaMode::Straight &&
        !ImageFormatHasAlpha(BaseMip.GetFormat()))
    {
        return EAssetResult::InvalidInput;
    }
    OutAsset.Id_ = std::move(Id);
    OutAsset.Source_ = std::move(Source);
    OutAsset.BaseMip_ = std::move(BaseMip);
    OutAsset.ColorSpace_ = ColorSpace;
    OutAsset.AlphaMode_ = AlphaMode;
    OutAsset.SourceDigest_ = SourceDigest;
    OutAsset.ContentDigest_ = DigestImage(
        OutAsset.Id_,
        OutAsset.BaseMip_,
        ColorSpace,
        AlphaMode);
    return EAssetResult::Success;
}

Core::FString FImageAsset::GetAssetType() const
{
    return TAssetTypeTraits<FImageAsset>::GetAssetType();
}

const FAssetId& FImageAsset::GetId() const noexcept { return Id_; }
const FAssetSourceLocator& FImageAsset::GetSource() const noexcept { return Source_; }
const FImageMip& FImageAsset::GetBaseMip() const noexcept { return BaseMip_; }
EImageColorSpace FImageAsset::GetColorSpace() const noexcept { return ColorSpace_; }
EImageAlphaMode FImageAsset::GetAlphaMode() const noexcept { return AlphaMode_; }
EImageOrigin FImageAsset::GetOrigin() const noexcept { return EImageOrigin::TopLeft; }
const FAssetDigest& FImageAsset::GetSourceDigest() const noexcept { return SourceDigest_; }
const FAssetDigest& FImageAsset::GetContentDigest() const noexcept { return ContentDigest_; }

EAssetResult FTextureAsset::Create(
    FAssetId Id,
    Core::TSharedPtr<const FImageAsset> Image,
    FImageImportSettings Settings,
    Core::TArray<FImageMip> Mips,
    FTextureAsset& OutAsset)
{
    OutAsset = {};
    if (!Id.IsValid() ||
        Id.GetAssetType() != TAssetTypeTraits<FTextureAsset>::GetAssetType() ||
        !Image ||
        Settings.Validate() != EAssetResult::Success ||
        Mips.empty() ||
        Mips.front().GetBytes().data() != Image->GetBaseMip().GetBytes().data())
    {
        return EAssetResult::InvalidInput;
    }

    EImageColorSpace ColorSpace =
        Settings.ColorSpace.value_or(Image->GetColorSpace());
    if (Settings.Semantic != ETextureSemantic::Color &&
        ColorSpace != EImageColorSpace::Linear)
    {
        return EAssetResult::InvalidInput;
    }
    Core::uint64 ChainBytes = 0;
    FImageExtent2D Expected = Image->GetBaseMip().GetExtent();
    for (const FImageMip& Mip : Mips)
    {
        const EAssetResult MipResult =
            Private::ValidateMip(Mip, Settings.Limits);
        if (MipResult != EAssetResult::Success)
        {
            return MipResult;
        }
        if (Mip.GetExtent() != Expected ||
            Mip.GetFormat() != Image->GetBaseMip().GetFormat() ||
            !Private::CheckedAdd(ChainBytes, Mip.GetBytes().size(), ChainBytes))
        {
            return EAssetResult::InvalidInput;
        }
        if (ChainBytes > Settings.Limits.MaxDecodedChainBytes)
        {
            return EAssetResult::ImageLimitExceeded;
        }
        Expected = Private::NextMipExtent(Expected);
    }
    const bool Complete =
        Mips.back().GetExtent() == FImageExtent2D{1, 1};
    if ((Settings.MipPolicy == EImageMipPolicy::BaseOnly && Mips.size() != 1) ||
        (Settings.MipPolicy == EImageMipPolicy::FullChain && !Complete))
    {
        return EAssetResult::InvalidInput;
    }

    TSoftAssetRef<FImageAsset> ImageReference;
    if (TSoftAssetRef<FImageAsset>::Create(Image->GetId(), ImageReference) !=
        EAssetResult::Success)
    {
        return EAssetResult::InvalidInput;
    }
    OutAsset.Id_ = std::move(Id);
    OutAsset.ImageReference_ = std::move(ImageReference);
    OutAsset.Image_ = std::move(Image);
    OutAsset.Settings_ = std::move(Settings);
    OutAsset.ColorSpace_ = ColorSpace;
    OutAsset.Mips_ = std::move(Mips);
    OutAsset.ContentDigest_ = DigestTexture(
        OutAsset.Id_,
        OutAsset.Settings_,
        OutAsset.ColorSpace_,
        OutAsset.Mips_);
    return EAssetResult::Success;
}

Core::FString FTextureAsset::GetAssetType() const
{
    return TAssetTypeTraits<FTextureAsset>::GetAssetType();
}

const FAssetId& FTextureAsset::GetId() const noexcept { return Id_; }
const TSoftAssetRef<FImageAsset>& FTextureAsset::GetImageReference() const noexcept
{
    return ImageReference_;
}
const Core::TSharedPtr<const FImageAsset>& FTextureAsset::GetImage() const noexcept
{
    return Image_;
}
const FImageImportSettings& FTextureAsset::GetSettings() const noexcept { return Settings_; }
ETextureSemantic FTextureAsset::GetSemantic() const noexcept { return Settings_.Semantic; }
EImageColorSpace FTextureAsset::GetColorSpace() const noexcept { return ColorSpace_; }
EImageAlphaMode FTextureAsset::GetAlphaMode() const noexcept
{
    return Image_ ? Image_->GetAlphaMode() : EImageAlphaMode::None;
}
EImageOrigin FTextureAsset::GetOrigin() const noexcept { return EImageOrigin::TopLeft; }
EImageMipPolicy FTextureAsset::GetMipPolicy() const noexcept { return Settings_.MipPolicy; }
const Core::TArray<FImageMip>& FTextureAsset::GetMips() const noexcept { return Mips_; }
const FAssetDigest& FTextureAsset::GetContentDigest() const noexcept { return ContentDigest_; }

} // namespace Stoner::Asset
