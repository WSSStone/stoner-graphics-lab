#pragma once

#include "Asset/FAssetManagerInspection.h"

namespace Stoner::Asset::Private
{

void NormalizeAssetManagerInspection(
    FAssetManagerInspection& Inspection,
    Core::uint32 MaximumRecords);

} // namespace Stoner::Asset::Private
