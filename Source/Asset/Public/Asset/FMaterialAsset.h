#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetVersion.h"
#include "Asset/FMaterialShaderTypes.h"

namespace Stoner::Asset
{

struct FMaterialAssetDesc
{
    FAssetId Id;
    FAssetVersion Version;
    Core::uint32 SchemaVersion = 1;
    EMaterialAssetDomain Domain = EMaterialAssetDomain::Surface;
    EMaterialAssetBlendMode BlendMode = EMaterialAssetBlendMode::Opaque;
    FMaterialAssetRenderState RenderState;
    TSoftAssetRef<FShaderAsset> Shader;
    FShaderPermutationKey PermutationRequest;
    Core::TArray<FMaterialAssetParameter> Parameters;
    Core::TArray<FAssetDependency> Dependencies;
    Core::FString CanonicalDefinition;
};

class FMaterialAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult CreateValidated(
        FMaterialAssetDesc Desc,
        FMaterialAsset& OutAsset,
        FAssetDiagnosticList* Diagnostics = nullptr);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FMaterialAssetDesc& GetDesc() const noexcept;

private:
    FMaterialAssetDesc Desc_;
};

} // namespace Stoner::Asset
