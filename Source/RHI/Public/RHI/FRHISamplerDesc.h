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
    return !(Desc.CompareMode != ERHISamplerCompareMode::None && Desc.MipFilter == ERHISamplerMipFilter::None);
}

} // namespace Stoner::RHI
