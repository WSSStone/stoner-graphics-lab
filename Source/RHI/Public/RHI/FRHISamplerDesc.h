#pragma once

#include "RHI/ERHISamplerMode.h"

namespace Stoner::RHI
{

struct FRHISamplerDesc
{
    ERHISamplerFilter MinFilter = ERHISamplerFilter::Linear;
    ERHISamplerFilter MagFilter = ERHISamplerFilter::Linear;
    ERHISamplerMipFilter MipFilter = ERHISamplerMipFilter::Linear;
    ERHISamplerAddressMode AddressU = ERHISamplerAddressMode::Repeat;
    ERHISamplerAddressMode AddressV = ERHISamplerAddressMode::Repeat;
    ERHISamplerAddressMode AddressW = ERHISamplerAddressMode::Repeat;
    ERHISamplerCompareMode CompareMode = ERHISamplerCompareMode::None;
};

[[nodiscard]] constexpr bool IsValidRHISamplerDesc(const FRHISamplerDesc& Desc) noexcept
{
    return IsValidRHISamplerFilter(Desc.MinFilter) &&
        IsValidRHISamplerFilter(Desc.MagFilter) &&
        IsValidRHISamplerMipFilter(Desc.MipFilter) &&
        IsValidRHISamplerAddressMode(Desc.AddressU) &&
        IsValidRHISamplerAddressMode(Desc.AddressV) &&
        IsValidRHISamplerAddressMode(Desc.AddressW) &&
        IsValidRHISamplerCompareMode(Desc.CompareMode) &&
        !(Desc.CompareMode != ERHISamplerCompareMode::None &&
            Desc.MipFilter == ERHISamplerMipFilter::None);
}

} // namespace Stoner::RHI
