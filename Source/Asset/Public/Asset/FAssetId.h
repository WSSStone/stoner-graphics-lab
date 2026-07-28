#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FString.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>

namespace Stoner::Asset
{

class FAssetId
{
public:
    FAssetId() = default;

    [[nodiscard]] static EAssetResult Create(
        const Core::FString& AssetType,
        const Core::FString& LogicalPath,
        const std::optional<Core::FString>& Subresource,
        FAssetId& OutId);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] const Core::FString& GetAssetType() const noexcept;
    [[nodiscard]] const Core::FString& GetLogicalPath() const noexcept;
    [[nodiscard]] const std::optional<Core::FString>& GetSubresource() const noexcept;
    [[nodiscard]] Core::FString ToString() const;
    [[nodiscard]] std::size_t GetLookupHash() const noexcept;

    [[nodiscard]] static bool CompareWithForcedCommonHashForTesting(
        const FAssetId& Left,
        const FAssetId& Right,
        std::size_t CommonHash) noexcept;

    [[nodiscard]] bool operator==(const FAssetId&) const = default;
    [[nodiscard]] bool operator<(const FAssetId& Other) const noexcept;

private:
    Core::FString AssetType_;
    Core::FString LogicalPath_;
    std::optional<Core::FString> Subresource_;
};

} // namespace Stoner::Asset

template <>
struct std::hash<Stoner::Asset::FAssetId>
{
    std::size_t operator()(const Stoner::Asset::FAssetId& Id) const noexcept
    {
        return Id.GetLookupHash();
    }
};
