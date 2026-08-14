#pragma once

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformTypes.h"
#include "Core/TArray.h"

#include <functional>

namespace Stoner::AssetCooker::Private
{

struct FCookInputSource
{
    Asset::FAssetSourceDescriptor Descriptor;
    Asset::FAssetSourceLease Source;
    Core::FString Role;
};

struct FCookInputRecord
{
    Asset::FAssetSourceLocator Locator;
    Core::FString Role;
    Core::uint64 ByteSize = 0;
    Asset::FAssetDigest Digest;
    Core::TSharedPtr<const Core::TArray<Core::uint8>> Bytes;

    [[nodiscard]] bool operator==(const FCookInputRecord&) const = default;
};

struct FCookInputSnapshot
{
    Core::TArray<FCookInputRecord> Records;
    Core::uint64 AggregateBytes = 0;
    Asset::FAssetDigest SnapshotDigest;
};

using FCookInputResolve = std::function<Asset::FAssetResolveResult(
    const Asset::FAssetSourceLocator&)>;

class FCookInputSnapshotBuilder
{
public:
    [[nodiscard]] static Asset::EAssetResult Pin(
        const Core::TArray<FCookInputSource>& Sources,
        Core::uint64 MaxSingleBytes,
        Core::uint64 MaxAggregateBytes,
        FCookInputSnapshot& OutSnapshot);

    [[nodiscard]] static Asset::EAssetResult Revalidate(
        const FCookInputSnapshot& Snapshot,
        Core::uint64 MaxSingleBytes,
        const FCookInputResolve& Resolve);
};

} // namespace Stoner::AssetCooker::Private
