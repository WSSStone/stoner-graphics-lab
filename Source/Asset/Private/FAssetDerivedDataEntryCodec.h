#pragma once

#include "Asset/FAssetDerivedDataEntry.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult WriteAssetDerivedDataEntry(
    const FAssetDerivedDataEntry& Entry,
    Core::FString& OutCanonical);

[[nodiscard]] EAssetResult ParseAssetDerivedDataEntry(
    std::span<const Core::uint8> Bytes,
    const FAssetDerivedDataEntryLimits& Limits,
    FAssetDerivedDataEntry& OutEntry);

} // namespace Stoner::Asset::Private
