#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSourceVersionRecord.h"
#include "Asset/FAssetVersion.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialShaderAssetLimits.h"
#include "Core/TSharedPtr.h"

#include <optional>

namespace Stoner::Asset
{

struct FMaterialInstanceAssetDesc
{
    FAssetId Id;
    FAssetVersion Version;
    Core::uint32 SchemaVersion = 1;
    FMaterialParentReference Parent;
    Core::TArray<FMaterialAssetParameter> Overrides;
    Core::TArray<FAssetDependency> Dependencies;
    Core::FString CanonicalDefinition;
};

class FMaterialInstanceAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult CreateValidated(
        FMaterialInstanceAssetDesc Desc,
        FMaterialInstanceAsset& OutAsset,
        FAssetDiagnosticList* Diagnostics = nullptr);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FMaterialInstanceAssetDesc& GetDesc() const noexcept;

private:
    FMaterialInstanceAssetDesc Desc_;
};

class IMaterialAssetLookup
{
public:
    virtual ~IMaterialAssetLookup() = default;
    [[nodiscard]] virtual Core::TSharedPtr<const FMaterialAsset> FindMaterial(
        const FAssetId& Id) const = 0;
    [[nodiscard]] virtual Core::TSharedPtr<const FMaterialInstanceAsset>
    FindInstance(const FAssetId& Id) const = 0;
    [[nodiscard]] virtual std::optional<FAssetVersion> FindDependencyVersion(
        const FAssetId& Id) const = 0;
};

struct FResolvedMaterialAsset
{
    FAssetId LeafId;
    FAssetVersion LeafVersion;
    FAssetId RootMaterialId;
    FAssetVersion RootMaterialVersion;
    Core::TArray<FAssetSourceVersionRecord> SourceManifest;
    EMaterialAssetDomain Domain = EMaterialAssetDomain::Surface;
    EMaterialAssetBlendMode BlendMode = EMaterialAssetBlendMode::Opaque;
    FMaterialAssetRenderState RenderState;
    TSoftAssetRef<FShaderAsset> Shader;
    FShaderPermutationKey PermutationRequest;
    Core::TArray<FMaterialAssetParameter> EffectiveParameters;
};

[[nodiscard]] EAssetResult ResolveMaterial(
    const FAssetId& MaterialOrInstance,
    const IMaterialAssetLookup& Lookup,
    const FMaterialShaderAssetLimits& Limits,
    FResolvedMaterialAsset& OutMaterial,
    FAssetDiagnosticList* Diagnostics = nullptr);

} // namespace Stoner::Asset
