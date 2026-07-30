#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FImageTypes.h"
#include "Asset/FTextureCook.h"
#include "Core/TArray.h"

#include <span>
#include <utility>

namespace Stoner::Asset::Private
{

struct FKTX2EncoderMip
{
    FImageExtent2D Extent;
    std::span<const Core::uint8> RGBA8Bytes;
};

struct FKTX2EncoderMetadata
{
    Core::FString Key;
    Core::TArray<Core::uint8> Value;
};

struct FKTX2EncoderRequest
{
    ETextureCompressionPolicy Policy =
        ETextureCompressionPolicy::ETC1S;
    ETextureCookQuality Quality = ETextureCookQuality::Balanced;
    bool bSRGB = false;
    bool bNormal = false;
    bool bForceAlpha = false;
    Core::TArray<FKTX2EncoderMip> Mips;
    Core::TArray<FKTX2EncoderMetadata> Metadata;
    Core::uint64 MaxOutputBytes =
        FTextureCookLimits::DefaultMaxArtifactBytes;
};

struct FKTX2EncoderResult
{
    EAssetResult Result = EAssetResult::CookFailure;
    Core::TArray<Core::uint8> Bytes;
    FAssetDiagnosticList Diagnostics;
};

class IKTX2Encoder
{
public:
    virtual ~IKTX2Encoder() = default;
    [[nodiscard]] virtual FKTX2EncoderResult Encode(
        const FKTX2EncoderRequest& Request) const = 0;
};

class FCanonicalBasisEncoder final : public IKTX2Encoder
{
public:
    [[nodiscard]] FKTX2EncoderResult Encode(
        const FKTX2EncoderRequest& Request) const override;
};

} // namespace Stoner::Asset::Private
