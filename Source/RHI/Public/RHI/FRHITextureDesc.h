#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResourceUsage.h"
#include "RHI/ERHITextureDimension.h"

namespace Stoner::RHI
{

struct FRHITextureDesc
{
    ERHITextureDimension Dimension = ERHITextureDimension::Texture2D;
    Stoner::Core::uint32 Width = 1;
    Stoner::Core::uint32 Height = 1;
    Stoner::Core::uint32 Depth = 1;
    Stoner::Core::uint32 MipLevels = 1;
    Stoner::Core::uint32 ArrayLayers = 1;
    ERHISampleCount SampleCount = ERHISampleCount::One;
    ERHIFormat Format = ERHIFormat::Unknown;
    ERHITextureUsage Usage = ERHITextureUsage::None;
};

[[nodiscard]] constexpr bool IsValidRHIUsage(ERHITextureUsage Usage) noexcept
{
    if (Usage == ERHITextureUsage::None || !HasOnlyRHIFlags(Usage, RHITextureUsageValidMask))
    {
        return false;
    }
    return !(HasRHIFlag(Usage, ERHITextureUsage::ColorAttachment) && HasRHIFlag(Usage, ERHITextureUsage::DepthStencilAttachment));
}

[[nodiscard]] constexpr Stoner::Core::uint32 GetRHIMaxMipLevels(
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::Core::uint32 Depth) noexcept
{
    Stoner::Core::uint32 LargestDimension = Width;
    if (Height > LargestDimension)
    {
        LargestDimension = Height;
    }
    if (Depth > LargestDimension)
    {
        LargestDimension = Depth;
    }

    Stoner::Core::uint32 MipLevels = 0;
    while (LargestDimension > 0)
    {
        ++MipLevels;
        LargestDimension >>= 1;
    }
    return MipLevels;
}

[[nodiscard]] constexpr Stoner::Core::uint32 GetRHIMipExtent(
    Stoner::Core::uint32 BaseExtent,
    Stoner::Core::uint32 MipLevel) noexcept
{
    if (BaseExtent == 0 || MipLevel >= 32)
    {
        return 0;
    }
    const Stoner::Core::uint32 Shifted = BaseExtent >> MipLevel;
    return Shifted > 0 ? Shifted : 1;
}

[[nodiscard]] constexpr bool IsRHITextureRegionValid(
    const FRHITextureDesc& Desc,
    Stoner::Core::uint32 MipLevel,
    Stoner::Core::uint32 ArrayLayer,
    Stoner::Core::uint32 X,
    Stoner::Core::uint32 Y,
    Stoner::Core::uint32 Z,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::Core::uint32 Depth) noexcept
{
    if (Width == 0 || Height == 0 || Depth == 0 ||
        MipLevel >= Desc.MipLevels || ArrayLayer >= Desc.ArrayLayers)
    {
        return false;
    }

    const Stoner::Core::uint32 MipWidth =
        GetRHIMipExtent(Desc.Width, MipLevel);
    const Stoner::Core::uint32 MipHeight =
        GetRHIMipExtent(Desc.Height, MipLevel);
    const Stoner::Core::uint32 MipDepth =
        GetRHIMipExtent(Desc.Depth, MipLevel);
    return X <= MipWidth && Width <= MipWidth - X &&
        Y <= MipHeight && Height <= MipHeight - Y &&
        Z <= MipDepth && Depth <= MipDepth - Z;
}

[[nodiscard]] constexpr bool IsValidRHITextureDesc(const FRHITextureDesc& Desc) noexcept
{
    if (!IsValidRHIFormat(Desc.Format) ||
        !IsValidRHIUsage(Desc.Usage) ||
        !IsValidRHISampleCount(Desc.SampleCount) ||
        Desc.MipLevels == 0 ||
        Desc.ArrayLayers == 0)
    {
        return false;
    }

    const bool bDepthStencilFormat = IsDepthStencilFormat(Desc.Format);
    if ((bDepthStencilFormat && HasRHIFlag(Desc.Usage, ERHITextureUsage::ColorAttachment)) ||
        (!bDepthStencilFormat && HasRHIFlag(Desc.Usage, ERHITextureUsage::DepthStencilAttachment)))
    {
        return false;
    }

    bool bValidDimensions = false;
    switch (Desc.Dimension)
    {
    case ERHITextureDimension::Texture1D:
        bValidDimensions = Desc.Width > 0 && Desc.Height == 1 && Desc.Depth == 1 && Desc.ArrayLayers == 1;
        break;
    case ERHITextureDimension::Texture2D:
        bValidDimensions = Desc.Width > 0 && Desc.Height > 0 && Desc.Depth == 1 && Desc.ArrayLayers == 1;
        break;
    case ERHITextureDimension::Texture3D:
        bValidDimensions = Desc.Width > 0 && Desc.Height > 0 && Desc.Depth > 0 && Desc.ArrayLayers == 1;
        break;
    case ERHITextureDimension::TextureCube:
        bValidDimensions = Desc.Width > 0 && Desc.Width == Desc.Height && Desc.Depth == 1 && Desc.ArrayLayers == 6;
        break;
    case ERHITextureDimension::Texture1DArray:
        bValidDimensions = Desc.Width > 0 && Desc.Height == 1 && Desc.Depth == 1 && Desc.ArrayLayers > 1;
        break;
    case ERHITextureDimension::Texture2DArray:
        bValidDimensions = Desc.Width > 0 && Desc.Height > 0 && Desc.Depth == 1 && Desc.ArrayLayers > 1;
        break;
    case ERHITextureDimension::TextureCubeArray:
        bValidDimensions = Desc.Width > 0 &&
            Desc.Width == Desc.Height &&
            Desc.Depth == 1 &&
            Desc.ArrayLayers >= 6 &&
            Desc.ArrayLayers % 6 == 0;
        break;
    }

    if (!bValidDimensions ||
        Desc.MipLevels > GetRHIMaxMipLevels(Desc.Width, Desc.Height, Desc.Depth))
    {
        return false;
    }
    return Desc.SampleCount == ERHISampleCount::One || Desc.MipLevels == 1;
}

} // namespace Stoner::RHI
