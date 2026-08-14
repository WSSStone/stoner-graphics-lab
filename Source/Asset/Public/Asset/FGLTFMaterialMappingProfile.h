#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

struct FGLTFMaterialMappingProfile
{
    Core::uint32 SchemaVersion = 1;
    Core::FString Name = Core::FString("gltf-metallic-roughness-v1");
    FAssetId SurfaceShader;
    Core::FString BaseColorFactor = Core::FString("BaseColorFactor");
    Core::FString MetallicFactor = Core::FString("MetallicFactor");
    Core::FString RoughnessFactor = Core::FString("RoughnessFactor");
    Core::FString EmissiveFactor = Core::FString("EmissiveFactor");
    Core::FString AlphaCutoff = Core::FString("AlphaCutoff");
    Core::FString NormalScale = Core::FString("NormalScale");
    Core::FString OcclusionStrength = Core::FString("OcclusionStrength");
    Core::FString BaseColorTexture = Core::FString("BaseColorTexture");
    Core::FString MetallicRoughnessTexture =
        Core::FString("MetallicRoughnessTexture");
    Core::FString NormalTexture = Core::FString("NormalTexture");
    Core::FString OcclusionTexture = Core::FString("OcclusionTexture");
    Core::FString EmissiveTexture = Core::FString("EmissiveTexture");

    [[nodiscard]] EAssetResult Validate() const;
    [[nodiscard]] FAssetDigest GetDigest() const;
};

[[nodiscard]] EAssetResult MakeDefaultGLTFMaterialMappingProfile(
    FGLTFMaterialMappingProfile& OutProfile);

} // namespace Stoner::Asset
