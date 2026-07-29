#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FPlatformTypes.h"

#include <optional>

namespace Stoner::Asset
{

struct FImageExtent2D
{
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Width > 0 && Height > 0;
    }

    [[nodiscard]] bool operator==(const FImageExtent2D&) const = default;
};

enum class EImageTexelFormat : Core::uint8
{
    Unknown,
    R8_UNorm,
    R8G8_UNorm,
    R8G8B8_UNorm,
    R8G8B8A8_UNorm,
    R32G32B32_Float,
    R16G16B16A16_Float,
    R32G32B32A32_Float
};

enum class ETextureSemantic : Core::uint8
{
    Color,
    Normal,
    Data,
    Unspecified
};

enum class EImageColorSpace : Core::uint8
{
    Linear,
    SRGB
};

enum class EImageAlphaMode : Core::uint8
{
    None,
    Straight
};

enum class EImageOrigin : Core::uint8
{
    TopLeft
};

enum class EImageMipPolicy : Core::uint8
{
    FullChain,
    BaseOnly
};

enum class EHDRLayout : Core::uint8
{
    DefaultRGBA16F,
    RGBA32F,
    RGB32F
};

enum class EImageSourceFormat : Core::uint8
{
    Unknown,
    PNG,
    JPEG,
    RadianceHDR
};

struct FImageImportLimits
{
    static constexpr Core::uint32 DefaultMaxDimension = 16384;
    static constexpr Core::uint64 DefaultMaxSourceBytes = 256ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint64 DefaultMaxMipBytes = 512ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint64 DefaultMaxDecodedChainBytes = 1024ULL * 1024ULL * 1024ULL;

    Core::uint32 MaxDimension = DefaultMaxDimension;
    Core::uint64 MaxSourceBytes = DefaultMaxSourceBytes;
    Core::uint64 MaxMipBytes = DefaultMaxMipBytes;
    Core::uint64 MaxDecodedChainBytes = DefaultMaxDecodedChainBytes;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FImageImportLimits&) const = default;
};

struct FImageImportSettings
{
    ETextureSemantic Semantic = ETextureSemantic::Unspecified;
    std::optional<EImageColorSpace> ColorSpace;
    EImageMipPolicy MipPolicy = EImageMipPolicy::FullChain;
    EHDRLayout HDRLayout = EHDRLayout::DefaultRGBA16F;
    FImageImportLimits Limits;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FImageImportSettings&) const = default;
};

[[nodiscard]] Core::uint32 GetImageChannelCount(EImageTexelFormat Format) noexcept;
[[nodiscard]] Core::uint32 GetImageBytesPerTexel(EImageTexelFormat Format) noexcept;
[[nodiscard]] bool IsImageFloatFormat(EImageTexelFormat Format) noexcept;
[[nodiscard]] bool ImageFormatHasAlpha(EImageTexelFormat Format) noexcept;

} // namespace Stoner::Asset
