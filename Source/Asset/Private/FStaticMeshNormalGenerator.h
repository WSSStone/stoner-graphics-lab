#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FStaticMeshTypes.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult GenerateFlatStaticMeshNormals(
    FStaticMeshVertexData& Vertices,
    FStaticMeshIndexData& Indices,
    Core::uint32 MaximumVertices);

} // namespace Stoner::Asset::Private
