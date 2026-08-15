#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/TArray.h"
#include "Core/TSharedPtr.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <utility>

namespace Stoner::Asset
{

namespace Private
{
class FAssetLoadOperationTable;
class FAssetNodeLoadCoordinator;
}

class FAssetCancellationToken
{
public:
    [[nodiscard]] bool IsCancellationRequested() const noexcept
    {
        if (Cancelled_.load(std::memory_order_acquire)) return true;
        std::lock_guard Lock(LinkedMutex_);
        if (Linked_.empty()) return false;
        for (const auto& Token : Linked_)
            if (Token && !Token->IsCancellationRequested()) return false;
        return true;
    }

private:
    friend class FAssetManager;
    friend class Private::FAssetLoadOperationTable;
    friend class Private::FAssetNodeLoadCoordinator;
    void RequestCancellation() noexcept
    {
        Cancelled_.store(true, std::memory_order_release);
    }
    void Link(Core::TSharedPtr<const FAssetCancellationToken> Token)
    {
        if (!Token || Token.get() == this) return;
        std::lock_guard Lock(LinkedMutex_);
        Linked_.push_back(std::move(Token));
    }
    std::atomic<bool> Cancelled_{false};
    mutable std::mutex LinkedMutex_;
    Core::TArray<Core::TSharedPtr<const FAssetCancellationToken>> Linked_;
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
