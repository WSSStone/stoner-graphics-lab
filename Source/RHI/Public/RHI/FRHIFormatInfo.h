#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"

#include <limits>

namespace Stoner::RHI
{

struct FRHIFormatInfo
{
    ERHIFormat Format = ERHIFormat::Unknown;
    Stoner::Core::uint8 BlockWidth = 0;
    Stoner::Core::uint8 BlockHeight = 0;
    Stoner::Core::uint8 BlockDepth = 0;
    Stoner::Core::uint8 BytesPerBlock = 0;
    bool bCompressed = false;
    bool bSRGB = false;
    bool bDepthStencil = false;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        if (!IsValidRHIFormat(Format) ||
            BlockWidth == 0 ||
            BlockHeight == 0 ||
            BlockDepth == 0 ||
            BytesPerBlock == 0 ||
            bDepthStencil != IsDepthStencilFormat(Format))
        {
            return false;
        }
        const bool bUnitBlock =
            BlockWidth == 1 &&
            BlockHeight == 1 &&
            BlockDepth == 1;
        return bCompressed != bUnitBlock;
    }
};

struct FRHITextureFootprint
{
    Stoner::Core::uint64 BlockCountX = 0;
    Stoner::Core::uint64 BlockCountY = 0;
    Stoner::Core::uint64 BlockCountZ = 0;
    Stoner::Core::uint64 TightRowBytes = 0;
    Stoner::Core::uint64 SliceBytes = 0;
    Stoner::Core::uint64 TotalBytes = 0;
};

[[nodiscard]] constexpr FRHIFormatInfo GetRHIFormatInfo(
    ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case ERHIFormat::R8_UNorm:
        return {Format, 1, 1, 1, 1, false, false, false};
    case ERHIFormat::R8G8_UNorm:
        return {Format, 1, 1, 1, 2, false, false, false};
    case ERHIFormat::R8G8B8A8_UNorm:
        return {Format, 1, 1, 1, 4, false, false, false};
    case ERHIFormat::R8G8B8A8_sRGB:
        return {Format, 1, 1, 1, 4, false, true, false};
    case ERHIFormat::B8G8R8A8_UNorm:
        return {Format, 1, 1, 1, 4, false, false, false};
    case ERHIFormat::R10G10B10A2_UNorm:
        return {Format, 1, 1, 1, 4, false, false, false};
    case ERHIFormat::R16G16B16A16_Float:
        return {Format, 1, 1, 1, 8, false, false, false};
    case ERHIFormat::R32_Float:
        return {Format, 1, 1, 1, 4, false, false, false};
    case ERHIFormat::R32G32_Float:
        return {Format, 1, 1, 1, 8, false, false, false};
    case ERHIFormat::R32G32B32_Float:
        return {Format, 1, 1, 1, 12, false, false, false};
    case ERHIFormat::R32G32B32A32_Float:
        return {Format, 1, 1, 1, 16, false, false, false};
    case ERHIFormat::BC1_RGBA_UNorm:
        return {Format, 4, 4, 1, 8, true, false, false};
    case ERHIFormat::BC1_RGBA_sRGB:
        return {Format, 4, 4, 1, 8, true, true, false};
    case ERHIFormat::BC3_RGBA_UNorm:
        return {Format, 4, 4, 1, 16, true, false, false};
    case ERHIFormat::BC3_RGBA_sRGB:
        return {Format, 4, 4, 1, 16, true, true, false};
    case ERHIFormat::BC4_R_UNorm:
        return {Format, 4, 4, 1, 8, true, false, false};
    case ERHIFormat::BC5_RG_UNorm:
        return {Format, 4, 4, 1, 16, true, false, false};
    case ERHIFormat::BC7_RGBA_UNorm:
        return {Format, 4, 4, 1, 16, true, false, false};
    case ERHIFormat::BC7_RGBA_sRGB:
        return {Format, 4, 4, 1, 16, true, true, false};
    case ERHIFormat::ETC2_RGB8_UNorm:
        return {Format, 4, 4, 1, 8, true, false, false};
    case ERHIFormat::ETC2_RGB8_sRGB:
        return {Format, 4, 4, 1, 8, true, true, false};
    case ERHIFormat::ETC2_RGBA8_UNorm:
        return {Format, 4, 4, 1, 16, true, false, false};
    case ERHIFormat::ETC2_RGBA8_sRGB:
        return {Format, 4, 4, 1, 16, true, true, false};
    case ERHIFormat::EAC_R11_UNorm:
        return {Format, 4, 4, 1, 8, true, false, false};
    case ERHIFormat::EAC_RG11_UNorm:
        return {Format, 4, 4, 1, 16, true, false, false};
    case ERHIFormat::ASTC_4x4_RGBA_UNorm:
        return {Format, 4, 4, 1, 16, true, false, false};
    case ERHIFormat::ASTC_4x4_RGBA_sRGB:
        return {Format, 4, 4, 1, 16, true, true, false};
    case ERHIFormat::D24_UNorm_S8_UInt:
        return {Format, 1, 1, 1, 4, false, false, true};
    case ERHIFormat::D32_Float:
        return {Format, 1, 1, 1, 4, false, false, true};
    case ERHIFormat::S8_UInt:
        return {Format, 1, 1, 1, 1, false, false, true};
    case ERHIFormat::Unknown:
    case ERHIFormat::Count:
        return {};
    }
    return {};
}

[[nodiscard]] constexpr bool TryGetRHITextureFootprint(
    ERHIFormat Format,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::Core::uint32 Depth,
    FRHITextureFootprint& OutFootprint) noexcept
{
    OutFootprint = {};
    const FRHIFormatInfo Info = GetRHIFormatInfo(Format);
    if (!Info.IsValid() ||
        Width == 0 ||
        Height == 0 ||
        Depth == 0)
    {
        return false;
    }

    const auto DivideRoundUp = [](
        Stoner::Core::uint64 Value,
        Stoner::Core::uint64 Divisor) constexpr
    {
        return Value / Divisor +
            static_cast<Stoner::Core::uint64>(
                Value % Divisor != 0);
    };
    constexpr Stoner::Core::uint64 MaxValue =
        std::numeric_limits<Stoner::Core::uint64>::max();
    const auto TryMultiply = [](
        Stoner::Core::uint64 Left,
        Stoner::Core::uint64 Right,
        Stoner::Core::uint64& OutValue) constexpr
    {
        if (Left != 0 && Right > MaxValue / Left)
        {
            return false;
        }
        OutValue = Left * Right;
        return true;
    };

    FRHITextureFootprint Candidate;
    Candidate.BlockCountX =
        DivideRoundUp(Width, Info.BlockWidth);
    Candidate.BlockCountY =
        DivideRoundUp(Height, Info.BlockHeight);
    Candidate.BlockCountZ =
        DivideRoundUp(Depth, Info.BlockDepth);
    if (!TryMultiply(
            Candidate.BlockCountX,
            Info.BytesPerBlock,
            Candidate.TightRowBytes) ||
        !TryMultiply(
            Candidate.TightRowBytes,
            Candidate.BlockCountY,
            Candidate.SliceBytes) ||
        !TryMultiply(
            Candidate.SliceBytes,
            Candidate.BlockCountZ,
            Candidate.TotalBytes))
    {
        return false;
    }
    OutFootprint = Candidate;
    return true;
}

} // namespace Stoner::RHI
