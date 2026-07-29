#pragma once

#include "FImageDecode.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult NormalizeTopLeft(
    EImageOrientationTransform Transform,
    FDecodedImageRaster& Raster);

} // namespace Stoner::Asset::Private
