#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSourceVersionRecord.h"
#include "Asset/FAssetVersion.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticMeshTypes.h"
#include "Asset/TSoftAssetRef.h"
#include "Core/FTransform.h"

#include <optional>

namespace Stoner::Asset
{

struct FStaticModelNode
{
    Core::FString StableKey;
    Core::FString DisplayName;
    Core::FTransform LocalTransform;
    Core::TArray<Core::uint32> Children;
    std::optional<TSoftAssetRef<FStaticMeshAsset>> Mesh;
    Core::uint32 SourceNodeIndex = 0;
    bool bNegativeDeterminant = false;
};

struct FStaticModelAssetDesc
{
    FAssetId Id;
    FAssetVersion Version;
    Core::uint32 SchemaVersion = 1;
    Core::FString SceneStableKey;
    bool bSourceDefaultScene = false;
    Core::TArray<FStaticModelNode> Nodes;
    Core::TArray<Core::uint32> RootNodeIndices;
    FStaticMeshBounds Bounds;
    Core::TArray<FAssetDependency> Dependencies;
    Core::TArray<FAssetSourceVersionRecord> SourceManifest;
    FAssetDigest ImportProfileDigest;
};

class FStaticModelAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult CreateValidated(
        FStaticModelAssetDesc Desc,
        Core::uint32 MaximumHierarchyDepth,
        FStaticModelAsset& OutAsset,
        FAssetDiagnosticList* Diagnostics = nullptr);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FStaticModelAssetDesc& GetDesc() const noexcept;

private:
    FStaticModelAssetDesc Desc_;
};

template <>
struct TAssetTypeTraits<FStaticModelAsset>
{
    static Core::FString GetAssetType() { return Core::FString("StaticModel"); }
};

} // namespace Stoner::Asset
