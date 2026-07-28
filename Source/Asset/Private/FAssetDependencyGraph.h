#pragma once

#include "Asset/FAssetMetadata.h"

#include <unordered_map>

namespace Stoner::Asset::Private
{

using FRecordMap = std::unordered_map<FAssetId, FAssetMetadata>;

[[nodiscard]] EAssetResult NormalizeAndValidateDependencyGraph(FRecordMap& Records);

} // namespace Stoner::Asset::Private
