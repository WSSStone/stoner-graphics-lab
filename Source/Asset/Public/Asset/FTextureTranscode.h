#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FKTX2TextureArtifact.h"
#include "Core/TSharedPtr.h"

namespace Stoner::Asset
{

enum class ETextureTranscodeFormat : Core::uint8
{
    Unknown,
    BC1_RGBA_UNorm,
    BC1_RGBA_SRGB,
    BC3_RGBA_UNorm,
    BC3_RGBA_SRGB,
    BC4_R_UNorm,
    BC5_RG_UNorm,
    BC7_RGBA_UNorm,
    BC7_RGBA_SRGB,
    ETC2_RGB8_UNorm,
    ETC2_RGB8_SRGB,
    ETC2_RGBA8_UNorm,
    ETC2_RGBA8_SRGB,
    EAC_R11_UNorm,
    EAC_RG11_UNorm,
    ASTC_4x4_RGBA_UNorm,
    ASTC_4x4_RGBA_SRGB,
    R8_UNorm,
    R8G8_UNorm,
    R8G8B8A8_UNorm,
    R8G8B8A8_SRGB
};

struct FTextureTranscodeRequest
{
    Core::TSharedPtr<const FKTX2TextureArtifact> Artifact;
    ETextureTranscodeFormat TargetFormat =
        ETextureTranscodeFormat::Unknown;
    FTextureCookLimits Limits;
};

struct FTranscodedTextureMip
{
    Core::uint32 MipLevel = 0;
    FImageExtent2D Extent;
    Core::uint32 BlockWidth = 0;
    Core::uint32 BlockHeight = 0;
    Core::uint32 BytesPerBlock = 0;
    Core::uint64 RowPitchBytes = 0;
    Core::TArray<Core::uint8> Bytes;
};

struct FTranscodedTexturePayload
{
    ETextureTranscodeFormat Format =
        ETextureTranscodeFormat::Unknown;
    ETextureSemantic Semantic = ETextureSemantic::Unspecified;
    EImageColorSpace ColorSpace = EImageColorSpace::Linear;
    EImageAlphaMode AlphaMode = EImageAlphaMode::None;
    EImageOrigin Origin = EImageOrigin::TopLeft;
    Core::TArray<FTranscodedTextureMip> Mips;
};

struct FTextureTranscodeResult
{
    EAssetResult Result = EAssetResult::Unsupported;
    Core::TSharedPtr<const FTranscodedTexturePayload> Payload;
    FAssetDiagnosticList Diagnostics;
};

class FTextureTranscoder
{
public:
    [[nodiscard]] static FTextureTranscodeResult Transcode(
        const FTextureTranscodeRequest& Request);
};

} // namespace Stoner::Asset
