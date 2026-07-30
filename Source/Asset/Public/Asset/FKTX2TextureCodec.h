#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FKTX2TextureArtifact.h"

namespace Stoner::Asset
{

class FKTX2TextureCodec
{
public:
    [[nodiscard]] static EAssetResult Inspect(
        std::span<const Core::uint8> Bytes,
        const FTextureCookLimits& Limits,
        FKTX2TextureInfo& OutInfo,
        FAssetDiagnosticList* OutDiagnostics = nullptr);

    [[nodiscard]] static EAssetResult Open(
        FAssetId ExpectedId,
        std::span<const Core::uint8> Bytes,
        const FTextureCookLimits& Limits,
        FKTX2TextureArtifact& OutArtifact,
        FAssetDiagnosticList* OutDiagnostics = nullptr);
};

} // namespace Stoner::Asset
