#pragma once

#include "Asset/FImageTypes.h"
#include "Core/TArray.h"
#include "Core/TSharedPtr.h"

#include <span>

namespace Stoner::Asset
{

class FImageMip
{
public:
    FImageMip() = default;

    [[nodiscard]] static EAssetResult Create(
        FImageExtent2D Extent,
        EImageTexelFormat Format,
        Core::TArray<Core::uint8> Bytes,
        FImageMip& OutMip);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] FImageExtent2D GetExtent() const noexcept;
    [[nodiscard]] EImageTexelFormat GetFormat() const noexcept;
    [[nodiscard]] Core::uint64 GetRowPitchBytes() const noexcept;
    [[nodiscard]] std::span<const Core::uint8> GetBytes() const noexcept;
    [[nodiscard]] const Core::TSharedPtr<const Core::TArray<Core::uint8>>&
        GetSharedBytes() const noexcept;

private:
    FImageExtent2D Extent_;
    EImageTexelFormat Format_ = EImageTexelFormat::Unknown;
    Core::uint64 RowPitchBytes_ = 0;
    Core::TSharedPtr<const Core::TArray<Core::uint8>> Bytes_;
};

} // namespace Stoner::Asset
