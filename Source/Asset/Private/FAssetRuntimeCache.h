#pragma once

#include "FAssetRuntimeTypes.h"
#include "Asset/TAssetHandle.h"

namespace Stoner::Asset::Private
{

struct FAssetRuntimeCacheSnapshot
{
    Core::uint32 Entries = 0;
    Core::uint64 PayloadBytes = 0;
    Core::uint64 ExternalHandles = 0;
    Core::uint64 RequestInterests = 0;
    Core::uint64 RequiredDependencies = 0;
};

struct FAssetRuntimeCacheEntrySnapshot
{
    FAssetLoadKey Key;
    Core::uint64 PayloadBytes = 0;
    FAssetRetentionCounts Retentions;
};

class FAssetRuntimeCache
{
public:
    explicit FAssetRuntimeCache(Core::uint64 MaximumPayloadBytes);
    ~FAssetRuntimeCache();
    FAssetRuntimeCache(const FAssetRuntimeCache&) = delete;
    FAssetRuntimeCache& operator=(const FAssetRuntimeCache&) = delete;

    [[nodiscard]] EAssetResult Publish(
        const FAssetLoadKey& Root,
        const FAssetLoadScratchResult& Loaded,
        Core::uint64 RequestInterests,
        Core::TSharedPtr<const FAssetPayload>& OutRootPayload);
    [[nodiscard]] bool AcquireRequest(
        const FAssetLoadKey& Key,
        Core::TSharedPtr<const FAssetPayload>& OutPayload);
    void ReleaseRequest(const FAssetLoadKey& Key);
    [[nodiscard]] EAssetResult AcquireExternal(
        const FAssetLoadKey& Key,
        Core::TSharedPtr<FAssetHandleControl>& OutControl);
    [[nodiscard]] FAssetRuntimeCacheSnapshot Inspect() const;
    [[nodiscard]] Core::TArray<FAssetRuntimeCacheEntrySnapshot>
    InspectEntries() const;
    void ClearManagerOwnership();

private:
    struct FState;
    Core::TSharedPtr<FState> State_;
};

} // namespace Stoner::Asset::Private
