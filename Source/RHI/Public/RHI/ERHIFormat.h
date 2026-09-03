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
    R10G10B10A2_UNorm,
    R16G16B16A16_Float,
    R32_Float,
    R32G32_Float,
    R32G32B32_Float,
    R32G32B32A32_Float,
    BC1_RGBA_UNorm,
    BC1_RGBA_sRGB,
    BC3_RGBA_UNorm,
    BC3_RGBA_sRGB,
    BC4_R_UNorm,
    BC5_RG_UNorm,
    BC7_RGBA_UNorm,
    BC7_RGBA_sRGB,
    ETC2_RGB8_UNorm,
    ETC2_RGB8_sRGB,
    ETC2_RGBA8_UNorm,
    ETC2_RGBA8_sRGB,
    EAC_R11_UNorm,
    EAC_RG11_UNorm,
    ASTC_4x4_RGBA_UNorm,
    ASTC_4x4_RGBA_sRGB,
    D24_UNorm_S8_UInt,
    D32_Float,
    S8_UInt,
    Count
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
    case ERHIFormat::R10G10B10A2_UNorm:
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
    case ERHIFormat::BC1_RGBA_UNorm:
    case ERHIFormat::BC1_RGBA_sRGB:
    case ERHIFormat::BC3_RGBA_UNorm:
    case ERHIFormat::BC3_RGBA_sRGB:
    case ERHIFormat::BC4_R_UNorm:
    case ERHIFormat::BC5_RG_UNorm:
    case ERHIFormat::BC7_RGBA_UNorm:
    case ERHIFormat::BC7_RGBA_sRGB:
    case ERHIFormat::ETC2_RGB8_UNorm:
    case ERHIFormat::ETC2_RGB8_sRGB:
    case ERHIFormat::ETC2_RGBA8_UNorm:
    case ERHIFormat::ETC2_RGBA8_sRGB:
    case ERHIFormat::EAC_R11_UNorm:
    case ERHIFormat::EAC_RG11_UNorm:
    case ERHIFormat::ASTC_4x4_RGBA_UNorm:
    case ERHIFormat::ASTC_4x4_RGBA_sRGB:
    case ERHIFormat::Unknown:
    case ERHIFormat::Count:
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
    case ERHIFormat::R10G10B10A2_UNorm:
    case ERHIFormat::R16G16B16A16_Float:
    case ERHIFormat::R32_Float:
    case ERHIFormat::R32G32_Float:
    case ERHIFormat::R32G32B32_Float:
    case ERHIFormat::R32G32B32A32_Float:
    case ERHIFormat::BC1_RGBA_UNorm:
    case ERHIFormat::BC1_RGBA_sRGB:
    case ERHIFormat::BC3_RGBA_UNorm:
    case ERHIFormat::BC3_RGBA_sRGB:
    case ERHIFormat::BC4_R_UNorm:
    case ERHIFormat::BC5_RG_UNorm:
    case ERHIFormat::BC7_RGBA_UNorm:
    case ERHIFormat::BC7_RGBA_sRGB:
    case ERHIFormat::ETC2_RGB8_UNorm:
    case ERHIFormat::ETC2_RGB8_sRGB:
    case ERHIFormat::ETC2_RGBA8_UNorm:
    case ERHIFormat::ETC2_RGBA8_sRGB:
    case ERHIFormat::EAC_R11_UNorm:
    case ERHIFormat::EAC_RG11_UNorm:
    case ERHIFormat::ASTC_4x4_RGBA_UNorm:
    case ERHIFormat::ASTC_4x4_RGBA_sRGB:
    case ERHIFormat::D24_UNorm_S8_UInt:
    case ERHIFormat::D32_Float:
    case ERHIFormat::S8_UInt:
        return true;
    case ERHIFormat::Unknown:
    case ERHIFormat::Count:
        return false;
    }
    return false;
}

[[nodiscard]] constexpr bool IsDepthStencilFormat(ERHIFormat Format) noexcept
{
    return Format == ERHIFormat::D24_UNorm_S8_UInt || Format == ERHIFormat::D32_Float || Format == ERHIFormat::S8_UInt;
}

} // namespace Stoner::RHI
