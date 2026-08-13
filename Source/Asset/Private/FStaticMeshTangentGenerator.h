#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FStaticMeshTypes.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult GenerateStaticMeshTangents(
    FStaticMeshVertexData& Vertices,
    FStaticMeshIndexData& Indices,
    Core::uint32 TexCoordSet,
    Core::uint32 MaximumVertices);

} // namespace Stoner::Asset::Private
