#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FStaticMeshTypes.h"

namespace Stoner::Asset::Private
{

class FGLTFGeometryNormalizer
{
public:
    [[nodiscard]] static EAssetResult ConvertPosition(
        const Core::FVector3& Source,
        Core::FVector3& OutPosition) noexcept;
    [[nodiscard]] static EAssetResult ConvertDirection(
        const Core::FVector3& Source,
        Core::FVector3& OutDirection) noexcept;
    [[nodiscard]] static EAssetResult ConvertTangent(
        const Core::FVector4& Source,
        Core::FVector4& OutTangent) noexcept;
    [[nodiscard]] static EAssetResult NormalizeSourceStreams(
        FStaticMeshVertexData& Vertices) noexcept;
};

} // namespace Stoner::Asset::Private
