#pragma once

#include "Asset/FAssetRequestHandle.h"
#include "Core/FPlatformTypes.h"
#include "Core/TUniquePtr.h"

namespace Stoner::Asset::Private
{

class FAssetCompletionQueue
{
public:
    explicit FAssetCompletionQueue(Core::uint32 Capacity);
    ~FAssetCompletionQueue();
    FAssetCompletionQueue(const FAssetCompletionQueue&) = delete;
    FAssetCompletionQueue& operator=(const FAssetCompletionQueue&) = delete;

    [[nodiscard]] bool Reserve();
    void ReleaseReservation();
    [[nodiscard]] bool Enqueue(
        Core::uint64 Sequence,
        FAssetRequestHandle Request,
        EAssetResult Result,
        FAssetCompletionCallback Callback);
    [[nodiscard]] bool Cancel(FAssetRequestHandle Request);
    [[nodiscard]] FAssetPumpResult Pump(Core::uint32 MaxCount);
    [[nodiscard]] Core::uint32 Reserved() const noexcept;
    [[nodiscard]] Core::uint32 Queued() const noexcept;
    void Discard() noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
};

} // namespace Stoner::Asset::Private
