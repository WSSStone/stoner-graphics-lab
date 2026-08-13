#pragma once

#include "Asset/FMaterialInstanceAsset.h"
#include "Renderer/FMaterial.h"
#include "Renderer/FMaterialResourceRequirement.h"
#include "Renderer/FShaderAssetConversion.h"
#include "RHI/FRHISamplerDesc.h"

namespace Stoner::Renderer
{

struct FMaterialAssetConversionRequest
{
    const Asset::FResolvedMaterialAsset* ResolvedMaterial = nullptr;
    const FShaderAssetSnapshot* Shader = nullptr;
};

struct FMaterialAssetSnapshot
{
    Core::TArray<Asset::FAssetSourceVersionRecord> SourceManifest;
    FMaterial Material;
    Core::TArray<FMaterialResourceRequirement> ResourceRequirements;
    struct FTextureBinding
    {
        Core::FString ParameterName;
        Asset::FAssetId TextureId;
        Core::uint32 TexCoordSet = 0;
        RHI::FRHISamplerDesc Sampler;
    };
    Core::TArray<FTextureBinding> TextureBindings;
};

[[nodiscard]] EMaterialResult ConvertMaterialSamplerIntent(
    const Asset::FMaterialSamplerIntent& Intent,
    RHI::FRHISamplerDesc& OutSampler);

[[nodiscard]] EMaterialResult ConvertMaterialAsset(
    const FMaterialAssetConversionRequest& Request,
    FMaterialAssetSnapshot& OutSnapshot,
    FMaterialDiagnosticLog* Diagnostics = nullptr);

} // namespace Stoner::Renderer
