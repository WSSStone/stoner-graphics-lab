#pragma once

#include "Asset/FCurrentGenerationPointer.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult WriteCurrentGenerationPointer(
    const FCurrentGenerationPointer& Pointer,
    Core::FString& OutCanonical);

[[nodiscard]] EAssetResult ParseCurrentGenerationPointer(
    std::span<const Core::uint8> Bytes,
    FCurrentGenerationPointer& OutPointer);

} // namespace Stoner::Asset::Private
