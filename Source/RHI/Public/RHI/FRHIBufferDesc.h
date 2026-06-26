#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResourceUsage.h"

namespace Stoner::RHI
{

struct FRHIBufferDesc
{
    Stoner::Core::uint64 SizeInBytes = 0;
    ERHIBufferUsage Usage = ERHIBufferUsage::None;
};

[[nodiscard]] constexpr bool IsValidRHIUsage(ERHIBufferUsage Usage) noexcept
{
    return Usage != ERHIBufferUsage::None && !HasRHIFlag(Usage, ERHIBufferUsage::ReservedPresent);
}

[[nodiscard]] constexpr bool IsValidRHIBufferDesc(const FRHIBufferDesc& Desc) noexcept
{
    return Desc.SizeInBytes > 0 && IsValidRHIUsage(Desc.Usage);
}

} // namespace Stoner::RHI
