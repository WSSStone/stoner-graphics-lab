#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FStaticMeshTypes.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult BuildStaticMeshBounds(
    std::span<const Core::FVector3> Positions,
    FStaticMeshBounds& OutBounds) noexcept;

[[nodiscard]] EAssetResult BuildAggregateStaticMeshBounds(
    std::span<const FStaticMeshPrimitive> Primitives,
    FStaticMeshBounds& OutBounds) noexcept;

} // namespace Stoner::Asset::Private
