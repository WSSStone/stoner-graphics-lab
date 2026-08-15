#pragma once

#include "FAssetRuntimeTypes.h"
#include "Core/TUniquePtr.h"

namespace Stoner::Asset::Private
{

class IAssetLoadingStrategy;

class FAssetNodeLoadCoordinator
{
public:
    FAssetNodeLoadCoordinator();
    ~FAssetNodeLoadCoordinator();
    FAssetNodeLoadCoordinator(const FAssetNodeLoadCoordinator&) = delete;
    FAssetNodeLoadCoordinator& operator=(const FAssetNodeLoadCoordinator&) = delete;

    [[nodiscard]] FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        IAssetLoadingStrategy& Strategy,
        const FAssetRuntimeExecutionContext& Context);
    [[nodiscard]] Core::uint32 ActiveEntries() const noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
};

} // namespace Stoner::Asset::Private
