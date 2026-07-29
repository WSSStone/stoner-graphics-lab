#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"
#include "Core/TSharedPtr.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace Stoner::Asset
{

class FAssetSourceLocator
{
public:
    [[nodiscard]] static EAssetResult Create(
        const Core::FString& Scheme,
        const Core::FString& Locator,
        FAssetSourceLocator& OutLocator);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] const Core::FString& GetScheme() const noexcept;
    [[nodiscard]] const Core::FString& GetLocator() const noexcept;
    [[nodiscard]] Core::FString ToString() const;
    [[nodiscard]] bool operator==(const FAssetSourceLocator&) const = default;
    [[nodiscard]] bool operator<(const FAssetSourceLocator& Other) const noexcept;

private:
    Core::FString Scheme_;
    Core::FString Locator_;
};

struct FAssetSourceDescriptor
{
    FAssetSourceLocator Location;
    std::optional<Core::uint64> Size;
    std::optional<Core::FString> FormatHint;

    [[nodiscard]] bool operator==(const FAssetSourceDescriptor&) const = default;
};

class IAssetSource
{
public:
    virtual ~IAssetSource() = default;
    [[nodiscard]] virtual EAssetResult Read(
        Core::uint64 Offset,
        Core::usize MaximumBytes,
        Core::TArray<Core::uint8>& OutBytes) const = 0;
};

class FAssetSourceLease
{
public:
    FAssetSourceLease() = default;
    explicit FAssetSourceLease(Core::TSharedPtr<const IAssetSource> Source)
        : Source_(std::move(Source))
    {
    }

    [[nodiscard]] bool IsValid() const noexcept { return Source_ != nullptr; }
    [[nodiscard]] EAssetResult ReadPrefix(
        Core::usize RequestedBytes,
        Core::TArray<Core::uint8>& OutBytes) const
    {
        OutBytes.clear();
        if (!Source_)
        {
            return EAssetResult::NotFound;
        }
        constexpr Core::usize MaximumProbeBytes = 64U * 1024U;
        return Source_->Read(0, std::min(RequestedBytes, MaximumProbeBytes), OutBytes);
    }

    [[nodiscard]] EAssetResult ReadRange(
        Core::uint64 Offset,
        Core::usize MaximumBytes,
        Core::TArray<Core::uint8>& OutBytes) const;

    [[nodiscard]] EAssetResult ReadBounded(
        Core::uint64 MaximumBytes,
        const std::optional<Core::uint64>& ExpectedSize,
        Core::TArray<Core::uint8>& OutBytes) const;

private:
    Core::TSharedPtr<const IAssetSource> Source_;
};

} // namespace Stoner::Asset
