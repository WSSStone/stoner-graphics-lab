#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResourceUsage.h"

namespace Stoner::RHI
{

enum class ERHIMemoryAccess
{
    DeviceLocal,
    HostVisible
};

struct FRHIBufferDesc
{
    Stoner::Core::uint64 SizeInBytes = 0;
    ERHIBufferUsage Usage = ERHIBufferUsage::None;
    ERHIMemoryAccess MemoryAccess = ERHIMemoryAccess::DeviceLocal;
};

[[nodiscard]] constexpr bool IsValidRHIMemoryAccess(ERHIMemoryAccess MemoryAccess) noexcept
{
    switch (MemoryAccess)
    {
    case ERHIMemoryAccess::DeviceLocal:
    case ERHIMemoryAccess::HostVisible:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidRHIUsage(ERHIBufferUsage Usage) noexcept
{
    return Usage != ERHIBufferUsage::None && HasOnlyRHIFlags(Usage, RHIBufferUsageValidMask);
}

[[nodiscard]] constexpr bool IsValidRHIBufferDesc(const FRHIBufferDesc& Desc) noexcept
{
    return Desc.SizeInBytes > 0 &&
        IsValidRHIUsage(Desc.Usage) &&
        IsValidRHIMemoryAccess(Desc.MemoryAccess);
}

} // namespace Stoner::RHI
