#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FImageMip.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult GenerateImageMips(
    const FImageMip& BaseMip,
    const FImageImportSettings& Settings,
    Core::TArray<FImageMip>& OutMips,
    FAssetDiagnostic* OutDiagnostic = nullptr);

} // namespace Stoner::Asset::Private
