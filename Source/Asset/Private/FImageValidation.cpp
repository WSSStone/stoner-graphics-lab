#include "FImageValidation.h"

#include <limits>

namespace Stoner::Asset
{

EAssetResult FImageImportLimits::Validate() const noexcept
{
    return MaxDimension > 0 &&
            MaxSourceBytes > 0 &&
            MaxMipBytes > 0 &&
            MaxDecodedChainBytes > 0
        ? EAssetResult::Success
        : EAssetResult::InvalidInput;
}

EAssetResult FImageImportSettings::Validate() const noexcept
{
    if (Limits.Validate() != EAssetResult::Success)
    {
        return EAssetResult::InvalidInput;
    }
    if (Semantic != ETextureSemantic::Color &&
        Semantic != ETextureSemantic::Normal &&
        Semantic != ETextureSemantic::Data)
    {
        return EAssetResult::InvalidInput;
    }
    if ((ColorSpace &&
         *ColorSpace != EImageColorSpace::Linear &&
         *ColorSpace != EImageColorSpace::SRGB) ||
        (MipPolicy != EImageMipPolicy::FullChain &&
         MipPolicy != EImageMipPolicy::BaseOnly) ||
        (HDRLayout != EHDRLayout::DefaultRGBA16F &&
         HDRLayout != EHDRLayout::RGBA32F &&
         HDRLayout != EHDRLayout::RGB32F))
    {
        return EAssetResult::InvalidInput;
    }
    if (Semantic != ETextureSemantic::Color &&
        ColorSpace == EImageColorSpace::SRGB)
    {
        return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

Core::uint32 GetImageChannelCount(EImageTexelFormat Format) noexcept
{
    switch (Format)
    {
    case EImageTexelFormat::R8_UNorm: return 1;
    case EImageTexelFormat::R8G8_UNorm: return 2;
    case EImageTexelFormat::R8G8B8_UNorm:
    case EImageTexelFormat::R32G32B32_Float: return 3;
    case EImageTexelFormat::R8G8B8A8_UNorm:
    case EImageTexelFormat::R16G16B16A16_Float:
    case EImageTexelFormat::R32G32B32A32_Float: return 4;
    case EImageTexelFormat::Unknown: return 0;
    }
    return 0;
}

Core::uint32 GetImageBytesPerTexel(EImageTexelFormat Format) noexcept
{
    switch (Format)
    {
    case EImageTexelFormat::R8_UNorm: return 1;
    case EImageTexelFormat::R8G8_UNorm: return 2;
    case EImageTexelFormat::R8G8B8_UNorm: return 3;
    case EImageTexelFormat::R8G8B8A8_UNorm: return 4;
    case EImageTexelFormat::R16G16B16A16_Float: return 8;
    case EImageTexelFormat::R32G32B32_Float: return 12;
    case EImageTexelFormat::R32G32B32A32_Float: return 16;
    case EImageTexelFormat::Unknown: return 0;
    }
    return 0;
}

bool IsImageFloatFormat(EImageTexelFormat Format) noexcept
{
    return Format == EImageTexelFormat::R32G32B32_Float ||
        Format == EImageTexelFormat::R16G16B16A16_Float ||
        Format == EImageTexelFormat::R32G32B32A32_Float;
}

bool ImageFormatHasAlpha(EImageTexelFormat Format) noexcept
{
    return Format == EImageTexelFormat::R8G8_UNorm ||
        Format == EImageTexelFormat::R8G8B8A8_UNorm ||
        Format == EImageTexelFormat::R16G16B16A16_Float ||
        Format == EImageTexelFormat::R32G32B32A32_Float;
}

EAssetResult FImageMip::Create(
    FImageExtent2D Extent,
    EImageTexelFormat Format,
    Core::TArray<Core::uint8> Bytes,
    FImageMip& OutMip)
{
    OutMip = {};
    Core::uint64 RowPitch = 0;
    Core::uint64 TotalBytes = 0;
    if (!Extent.IsValid() ||
        Format == EImageTexelFormat::Unknown ||
        !Private::CheckedMultiply(
            Extent.Width,
            GetImageBytesPerTexel(Format),
            RowPitch) ||
        !Private::CheckedMultiply(RowPitch, Extent.Height, TotalBytes) ||
        TotalBytes != Bytes.size())
    {
        return EAssetResult::InvalidInput;
    }
    OutMip.Extent_ = Extent;
    OutMip.Format_ = Format;
    OutMip.RowPitchBytes_ = RowPitch;
    OutMip.Bytes_ =
        Core::MakeShared<const Core::TArray<Core::uint8>>(std::move(Bytes));
    return EAssetResult::Success;
}

bool FImageMip::IsValid() const noexcept
{
    return Extent_.IsValid() &&
        Format_ != EImageTexelFormat::Unknown &&
        RowPitchBytes_ > 0 &&
        Bytes_ != nullptr &&
        !Bytes_->empty();
}

FImageExtent2D FImageMip::GetExtent() const noexcept { return Extent_; }
EImageTexelFormat FImageMip::GetFormat() const noexcept { return Format_; }
Core::uint64 FImageMip::GetRowPitchBytes() const noexcept { return RowPitchBytes_; }

std::span<const Core::uint8> FImageMip::GetBytes() const noexcept
{
    return Bytes_ ? std::span<const Core::uint8>(*Bytes_)
                  : std::span<const Core::uint8>{};
}

const Core::TSharedPtr<const Core::TArray<Core::uint8>>&
FImageMip::GetSharedBytes() const noexcept
{
    return Bytes_;
}

} // namespace Stoner::Asset

namespace Stoner::Asset::Private
{

bool CheckedMultiply(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept
{
    OutValue = 0;
    if (Left != 0 &&
        Right > std::numeric_limits<Core::uint64>::max() / Left)
    {
        return false;
    }
    OutValue = Left * Right;
    return true;
}

bool CheckedAdd(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept
{
    OutValue = 0;
    if (Right > std::numeric_limits<Core::uint64>::max() - Left)
    {
        return false;
    }
    OutValue = Left + Right;
    return true;
}

EAssetResult ValidateMip(
    const FImageMip& Mip,
    const FImageImportLimits& Limits) noexcept
{
    if (!Mip.IsValid())
    {
        return EAssetResult::InvalidInput;
    }
    if (Mip.GetExtent().Width > Limits.MaxDimension ||
        Mip.GetExtent().Height > Limits.MaxDimension ||
        Mip.GetBytes().size() > Limits.MaxMipBytes)
    {
        return EAssetResult::ImageLimitExceeded;
    }
    return EAssetResult::Success;
}

FImageExtent2D NextMipExtent(FImageExtent2D Extent) noexcept
{
    return {
        Extent.Width > 1 ? Extent.Width / 2 : 1,
        Extent.Height > 1 ? Extent.Height / 2 : 1};
}

} // namespace Stoner::Asset::Private
