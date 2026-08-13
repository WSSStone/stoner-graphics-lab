#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSourceVersionRecord.h"
#include "Asset/FAssetVersion.h"
#include "Asset/FStaticMeshTypes.h"

namespace Stoner::Asset
{

struct FStaticMeshAssetDesc
{
    FAssetId Id;
    FAssetVersion Version;
    Core::uint32 SchemaVersion = 1;
    Core::TArray<FStaticMeshPrimitive> Primitives;
    Core::TArray<FStaticMeshMaterialSlot> MaterialSlots;
    FStaticMeshBounds Bounds;
    Core::TArray<FAssetDependency> Dependencies;
    Core::TArray<FAssetSourceVersionRecord> SourceManifest;
    FAssetDigest ImportProfileDigest;
};

class FStaticMeshAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult CreateValidated(
        FStaticMeshAssetDesc Desc,
        FStaticMeshAsset& OutAsset,
        FAssetDiagnosticList* Diagnostics = nullptr);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FStaticMeshAssetDesc& GetDesc() const noexcept;

private:
    FStaticMeshAssetDesc Desc_;
};

template <>
struct TAssetTypeTraits<FStaticMeshAsset>
{
    static Core::FString GetAssetType() { return Core::FString("StaticMesh"); }
};

} // namespace Stoner::Asset
