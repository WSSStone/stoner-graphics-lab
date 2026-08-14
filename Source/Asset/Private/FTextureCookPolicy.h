#pragma once

#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FTextureAsset.h"
#include "IKTX2Encoder.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ResolveTextureProfileSettings(
    const Core::TSharedPtr<const FAssetTargetProfileEvidence>& Profile,
    const FTextureCookSettings& LegacySettings,
    FTextureCookSettings& OutSettings,
    FAssetProfileProjectionEvidence& OutProjection,
    FAssetDiagnosticList* OutDiagnostics = nullptr);

[[nodiscard]] EAssetResult ResolveTextureCookPolicy(
    const FTextureAsset& Texture,
    const FTextureCookSettings& Settings,
    ETextureCompressionPolicy& OutPolicy,
    FAssetDiagnosticList* OutDiagnostics = nullptr);

[[nodiscard]] FAssetDigest BuildTextureCookRevision(
    const FTextureAsset& Texture,
    const FTextureCookSettings& Settings,
    ETextureCompressionPolicy ResolvedPolicy);

[[nodiscard]] Core::TArray<FKTX2EncoderMetadata>
BuildKTX2Metadata(
    const FTextureAsset& Texture,
    const FTextureCookSettings& Settings,
    ETextureCompressionPolicy ResolvedPolicy,
    const FAssetDigest& CookRevision);

[[nodiscard]] const char* TexturePolicyToken(
    ETextureCompressionPolicy Policy) noexcept;
[[nodiscard]] const char* TextureSemanticToken(
    ETextureSemantic Semantic) noexcept;

} // namespace Stoner::Asset::Private
