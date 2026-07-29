#pragma once

namespace Stoner::RHI
{

enum class ERHIFormat
{
    Unknown,
    R8_UNorm,
    R8G8_UNorm,
    R8G8B8A8_UNorm,
    R8G8B8A8_sRGB,
    B8G8R8A8_UNorm,
    R16G16B16A16_Float,
    R32_Float,
    R32G32_Float,
    R32G32B32_Float,
    R32G32B32A32_Float,
    D24_UNorm_S8_UInt,
    D32_Float,
    S8_UInt
};

[[nodiscard]] constexpr unsigned int GetRHIFormatByteSize(
    ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case ERHIFormat::R8_UNorm:
    case ERHIFormat::S8_UInt:
        return 1;
    case ERHIFormat::R8G8_UNorm:
        return 2;
    case ERHIFormat::R8G8B8A8_UNorm:
    case ERHIFormat::R8G8B8A8_sRGB:
    case ERHIFormat::B8G8R8A8_UNorm:
    case ERHIFormat::R32_Float:
    case ERHIFormat::D24_UNorm_S8_UInt:
    case ERHIFormat::D32_Float:
        return 4;
    case ERHIFormat::R16G16B16A16_Float:
    case ERHIFormat::R32G32_Float:
        return 8;
    case ERHIFormat::R32G32B32_Float:
        return 12;
    case ERHIFormat::R32G32B32A32_Float:
        return 16;
    case ERHIFormat::Unknown:
        return 0;
    }
    return 0;
}

[[nodiscard]] constexpr bool IsValidRHIFormat(ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case ERHIFormat::R8_UNorm:
    case ERHIFormat::R8G8_UNorm:
    case ERHIFormat::R8G8B8A8_UNorm:
    case ERHIFormat::R8G8B8A8_sRGB:
    case ERHIFormat::B8G8R8A8_UNorm:
    case ERHIFormat::R16G16B16A16_Float:
    case ERHIFormat::R32_Float:
    case ERHIFormat::R32G32_Float:
    case ERHIFormat::R32G32B32_Float:
    case ERHIFormat::R32G32B32A32_Float:
    case ERHIFormat::D24_UNorm_S8_UInt:
    case ERHIFormat::D32_Float:
    case ERHIFormat::S8_UInt:
        return true;
    case ERHIFormat::Unknown:
        return false;
    }
    return false;
}

[[nodiscard]] constexpr bool IsDepthStencilFormat(ERHIFormat Format) noexcept
{
    return Format == ERHIFormat::D24_UNorm_S8_UInt || Format == ERHIFormat::D32_Float || Format == ERHIFormat::S8_UInt;
}

} // namespace Stoner::RHI
