#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/TSharedPtr.h"

#include <atomic>
#include <chrono>

namespace Stoner::Asset
{

namespace Private
{
class FAssetLoadOperationTable;
}

class FAssetCancellationToken
{
public:
    [[nodiscard]] bool IsCancellationRequested() const noexcept
    {
        return Cancelled_.load(std::memory_order_acquire);
    }

private:
    friend class FAssetManager;
    friend class Private::FAssetLoadOperationTable;
    void RequestCancellation() noexcept
    {
        Cancelled_.store(true, std::memory_order_release);
    }
    std::atomic<bool> Cancelled_{false};
};

struct FAssetRuntimeExecutionContext
{
    Core::TSharedPtr<const FAssetCancellationToken> Cancellation;
    std::chrono::steady_clock::time_point Deadline;

    [[nodiscard]] bool ShouldStop() const noexcept
    {
        return !Cancellation || Cancellation->IsCancellationRequested() ||
            std::chrono::steady_clock::now() >= Deadline;
    }
};

} // namespace Stoner::Asset
