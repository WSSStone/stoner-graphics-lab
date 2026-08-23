#pragma once

#include "FAssetRuntimeTypes.h"

#include <atomic>
#include <limits>

namespace Stoner::Asset::Private
{

struct FAssetManagerExecutionCounterState
{
    std::atomic<Core::uint64> ResolverExecutions{0};
    std::atomic<Core::uint64> ImporterExecutions{0};
    std::atomic<Core::uint64> AuthoringDecoderExecutions{0};
    std::atomic<Core::uint64> SourceFallbackExecutions{0};
    std::atomic<Core::uint64> StrictLoaderExecutions{0};

    static void Increment(std::atomic<Core::uint64>& Counter) noexcept
    {
        Core::uint64 Current = Counter.load(std::memory_order_relaxed);
        while (Current != std::numeric_limits<Core::uint64>::max() &&
               !Counter.compare_exchange_weak(
                   Current,
                   Current + 1,
                   std::memory_order_relaxed,
                   std::memory_order_relaxed))
        {
        }
    }
};

class IAssetLoadingStrategy
{
public:
    virtual ~IAssetLoadingStrategy() = default;
    [[nodiscard]] virtual FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        const FAssetRuntimeExecutionContext& Context) = 0;
};

} // namespace Stoner::Asset::Private
