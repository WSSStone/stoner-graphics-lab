#pragma once

namespace Stoner::RHI
{

enum class ERHIFormat
{
    Unknown,
    R8_UNorm,
    R8G8B8A8_UNorm,
    B8G8R8A8_UNorm,
    R16G16B16A16_Float,
    R32_Float,
    R32G32_Float,
    R32G32B32_Float,
    D24_UNorm_S8_UInt,
    D32_Float,
    S8_UInt
};

[[nodiscard]] constexpr bool IsDepthStencilFormat(ERHIFormat Format) noexcept
{
    return Format == ERHIFormat::D24_UNorm_S8_UInt || Format == ERHIFormat::D32_Float || Format == ERHIFormat::S8_UInt;
}

} // namespace Stoner::RHI
