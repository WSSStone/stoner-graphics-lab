#pragma once

#include "Asset/FImageMip.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] bool CheckedMultiply(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept;

[[nodiscard]] bool CheckedAdd(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept;

[[nodiscard]] EAssetResult ValidateMip(
    const FImageMip& Mip,
    const FImageImportLimits& Limits) noexcept;

[[nodiscard]] FImageExtent2D NextMipExtent(FImageExtent2D Extent) noexcept;

} // namespace Stoner::Asset::Private
