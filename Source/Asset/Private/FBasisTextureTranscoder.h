#pragma once

#include "Asset/FTextureTranscode.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult TranscodeBasisTexture(
    const FKTX2TextureArtifact& Artifact,
    ETextureTranscodeFormat TargetFormat,
    const FTextureCookLimits& Limits,
    FTranscodedTexturePayload& OutPayload,
    FAssetDiagnosticList& OutDiagnostics);

} // namespace Stoner::Asset::Private
