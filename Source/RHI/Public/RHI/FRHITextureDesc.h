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
    if (Usage == ERHITextureUsage::None || HasRHIFlag(Usage, ERHITextureUsage::Vertex))
    {
        return false;
    }
    return !(HasRHIFlag(Usage, ERHITextureUsage::ColorAttachment) && HasRHIFlag(Usage, ERHITextureUsage::DepthStencilAttachment));
}

[[nodiscard]] constexpr bool IsValidRHITextureDesc(const FRHITextureDesc& Desc) noexcept
{
    if (Desc.Format == ERHIFormat::Unknown || !IsValidRHIUsage(Desc.Usage) || Desc.MipLevels == 0 || Desc.ArrayLayers == 0)
    {
        return false;
    }

    switch (Desc.Dimension)
    {
    case ERHITextureDimension::Texture1D:
        return Desc.Width > 0 && Desc.Height == 1 && Desc.Depth == 1 && Desc.ArrayLayers == 1;
    case ERHITextureDimension::Texture2D:
        return Desc.Width > 0 && Desc.Height > 0 && Desc.Depth == 1 && Desc.ArrayLayers == 1;
    case ERHITextureDimension::Texture3D:
        return Desc.Width > 0 && Desc.Height > 0 && Desc.Depth > 0 && Desc.ArrayLayers == 1;
    case ERHITextureDimension::TextureCube:
        return Desc.Width > 0 && Desc.Width == Desc.Height && Desc.Depth == 1 && Desc.ArrayLayers == 6;
    case ERHITextureDimension::Texture1DArray:
        return Desc.Width > 0 && Desc.Height == 1 && Desc.Depth == 1 && Desc.ArrayLayers > 1;
    case ERHITextureDimension::Texture2DArray:
        return Desc.Width > 0 && Desc.Height > 0 && Desc.Depth == 1 && Desc.ArrayLayers > 1;
    case ERHITextureDimension::TextureCubeArray:
        return Desc.Width > 0 && Desc.Width == Desc.Height && Desc.Depth == 1 && Desc.ArrayLayers >= 6 && Desc.ArrayLayers % 6 == 0;
    }
    return false;
}

} // namespace Stoner::RHI
