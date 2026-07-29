#pragma once

#include "Asset/FImageInspection.h"

#include <span>

namespace Stoner::Asset::Private
{

class FImageContainerInspector
{
public:
    [[nodiscard]] static EImageSourceFormat Detect(
        std::span<const Core::uint8> Prefix) noexcept;

    [[nodiscard]] static EAssetResult Inspect(
        std::span<const Core::uint8> Bytes,
        const FImageImportLimits& Limits,
        FImageContainerInspection& OutInspection,
        FAssetDiagnostic* OutDiagnostic = nullptr);
};

} // namespace Stoner::Asset::Private
