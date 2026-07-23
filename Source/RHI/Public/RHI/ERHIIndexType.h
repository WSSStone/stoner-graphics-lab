#pragma once

namespace Stoner::RHI
{

enum class ERHIIndexType
{
    UInt16,
    UInt32
};

[[nodiscard]] constexpr unsigned int GetRHIIndexTypeSize(ERHIIndexType Type) noexcept
{
    return Type == ERHIIndexType::UInt16 ? 2u : 4u;
}

} // namespace Stoner::RHI
