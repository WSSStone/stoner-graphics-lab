#include "FAssetCompletionQueue.h"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace Stoner::Asset::Private
{

struct FAssetCompletionQueue::FImpl
{
    struct FRecord
    {
        Core::uint64 Sequence = 0;
        FAssetRequestHandle Request;
        EAssetResult Result = EAssetResult::NotReady;
        FAssetCompletionCallback Callback;
    };

    explicit FImpl(Core::uint32 InCapacity) : Capacity(InCapacity)
    {
        Queue.reserve(InCapacity);
    }

    mutable std::mutex Mutex;
    std::vector<FRecord> Queue;
    Core::uint32 Capacity = 0;
    Core::uint32 Reservations = 0;
    bool bPumping = false;
};

FAssetCompletionQueue::FAssetCompletionQueue(Core::uint32 Capacity)
    : Impl_(Core::MakeUnique<FImpl>(Capacity))
{
}

FAssetCompletionQueue::~FAssetCompletionQueue()
{
    Discard();
}

bool FAssetCompletionQueue::Reserve()
{
    std::lock_guard Lock(Impl_->Mutex);
    if (Impl_->Reservations >= Impl_->Capacity) return false;
    ++Impl_->Reservations;
    return true;
}

void FAssetCompletionQueue::ReleaseReservation()
{
    std::lock_guard Lock(Impl_->Mutex);
    if (Impl_->Reservations > 0) --Impl_->Reservations;
}

bool FAssetCompletionQueue::Enqueue(
    Core::uint64 Sequence,
    FAssetRequestHandle Request,
    EAssetResult Result,
    FAssetCompletionCallback Callback)
{
    if (!Request.IsValid() || !Callback || Sequence == 0) return false;
    std::lock_guard Lock(Impl_->Mutex);
    if (Impl_->Reservations == 0 || Impl_->Queue.size() >= Impl_->Capacity)
        return false;
    Impl_->Queue.push_back(
        {Sequence, Request, Result, std::move(Callback)});
    return true;
}

bool FAssetCompletionQueue::Cancel(FAssetRequestHandle Request)
{
    std::lock_guard Lock(Impl_->Mutex);
    const auto Found = std::find_if(Impl_->Queue.begin(), Impl_->Queue.end(),
        [Request](const FImpl::FRecord& Record)
        {
            return Record.Request == Request;
        });
    if (Found == Impl_->Queue.end()) return false;
    Impl_->Queue.erase(Found);
    if (Impl_->Reservations > 0) --Impl_->Reservations;
    return true;
}

FAssetPumpResult FAssetCompletionQueue::Pump(Core::uint32 MaxCount)
{
    if (MaxCount == 0)
        return {EAssetResult::InvalidInput, 0};
    std::vector<FImpl::FRecord> Dispatch;
    {
        std::lock_guard Lock(Impl_->Mutex);
        if (Impl_->bPumping)
            return {EAssetResult::ReentrantPump, 0};
        Impl_->bPumping = true;
        const Core::usize Count = std::min<Core::usize>(
            MaxCount, Impl_->Queue.size());
        Dispatch.reserve(Count);
        for (Core::usize Index = 0; Index < Count; ++Index)
            Dispatch.push_back(std::move(Impl_->Queue[Index]));
        Impl_->Queue.erase(Impl_->Queue.begin(),
            Impl_->Queue.begin() + static_cast<std::ptrdiff_t>(Count));
        Impl_->Reservations -= static_cast<Core::uint32>(Count);
    }
    for (auto& Record : Dispatch)
        Record.Callback(Record.Request, Record.Result);
    {
        std::lock_guard Lock(Impl_->Mutex);
        Impl_->bPumping = false;
    }
    return {EAssetResult::Success,
        static_cast<Core::uint32>(Dispatch.size())};
}

Core::uint32 FAssetCompletionQueue::Reserved() const noexcept
{
    std::lock_guard Lock(Impl_->Mutex);
    return Impl_->Reservations;
}

Core::uint32 FAssetCompletionQueue::Queued() const noexcept
{
    std::lock_guard Lock(Impl_->Mutex);
    return static_cast<Core::uint32>(Impl_->Queue.size());
}

void FAssetCompletionQueue::Discard() noexcept
{
    std::lock_guard Lock(Impl_->Mutex);
    Impl_->Queue.clear();
    Impl_->Reservations = 0;
}

} // namespace Stoner::Asset::Private
