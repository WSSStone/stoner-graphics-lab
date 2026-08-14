#pragma once

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformTypes.h"
#include "Core/TArray.h"

namespace Stoner::AssetCooker::Private
{

struct FAssetCookGraphLimits
{
    Core::uint32 MaxAssets = 100000;
    Core::uint32 MaxDependencyEdges = 1000000;
    Core::uint32 MaxDependencyDepth = 256;

    [[nodiscard]] Asset::EAssetResult Validate() const noexcept;
};

struct FAssetCookGraphNode
{
    Core::uint32 PlanIndex = 0;
    Asset::FAssetMetadata Metadata;
    Core::TSharedPtr<const Asset::FAssetPayload> Payload;
    Core::TArray<Core::uint32> Dependencies;
    Core::TArray<Core::uint32> Dependents;
};

struct FAssetCookGraphPlan
{
    Core::TArray<Asset::FAssetId> Roots;
    Core::TArray<FAssetCookGraphNode> Nodes;
    Core::uint64 DependencyEdges = 0;

    [[nodiscard]] Asset::EAssetResult Validate() const noexcept;
};

class FAssetCookGraph
{
public:
    [[nodiscard]] static Asset::EAssetResult Build(
        const Core::TArray<Asset::FAssetImportOutput>& AvailableOutputs,
        Asset::EAssetCookSelectionMode SelectionMode,
        const Core::TArray<Asset::FAssetId>& ExplicitRoots,
        const FAssetCookGraphLimits& Limits,
        FAssetCookGraphPlan& OutPlan);
};

} // namespace Stoner::AssetCooker::Private
