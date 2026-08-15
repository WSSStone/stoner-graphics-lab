#include "FAssetNodeLoadCoordinator.h"

#include "IAssetLoadingStrategy.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace Stoner::Asset::Private
{

struct FAssetNodeLoadCoordinator::FImpl
{
    struct FEntry
    {
        FAssetLoadKey Key;
        FAssetLoadScratchResult Result;
        Core::TSharedPtr<FAssetCancellationToken> PhysicalCancellation;
        Core::uint32 Interests = 0;
        bool bComplete = false;
        std::condition_variable Condition;
    };

    void Release(const Core::TSharedPtr<FEntry>& Entry)
    {
        if (Entry->Interests > 0) --Entry->Interests;
        if (Entry->Interests == 0 && Entry->bComplete)
            Entries.erase(std::remove(Entries.begin(), Entries.end(), Entry),
                Entries.end());
    }

    mutable std::mutex Mutex;
    std::vector<Core::TSharedPtr<FEntry>> Entries;
};

FAssetNodeLoadCoordinator::FAssetNodeLoadCoordinator()
    : Impl_(Core::MakeUnique<FImpl>())
{
}

FAssetNodeLoadCoordinator::~FAssetNodeLoadCoordinator() = default;

FAssetLoadScratchResult FAssetNodeLoadCoordinator::Load(
    const FAssetLoadKey& Key,
    IAssetLoadingStrategy& Strategy,
    const FAssetRuntimeExecutionContext& Context)
{
    Core::TSharedPtr<FImpl::FEntry> Entry;
    bool bOwner = false;
    {
        std::lock_guard Lock(Impl_->Mutex);
        const auto Found = std::find_if(
            Impl_->Entries.begin(), Impl_->Entries.end(),
            [&Key](const auto& Value) { return Value->Key == Key; });
        if (Found == Impl_->Entries.end())
        {
            Entry = Core::MakeShared<FImpl::FEntry>();
            Entry->Key = Key;
            Entry->PhysicalCancellation =
                Core::MakeShared<FAssetCancellationToken>();
            Entry->PhysicalCancellation->Link(Context.Cancellation);
            Entry->Interests = 1;
            Impl_->Entries.push_back(Entry);
            bOwner = true;
        }
        else
        {
            Entry = *Found;
            Entry->PhysicalCancellation->Link(Context.Cancellation);
            ++Entry->Interests;
        }
    }

    if (bOwner)
    {
        const FAssetRuntimeExecutionContext PhysicalContext{
            Entry->PhysicalCancellation, Context.Deadline};
        FAssetLoadScratchResult Loaded = Strategy.Load(Key, PhysicalContext);
        std::lock_guard Lock(Impl_->Mutex);
        Entry->Result = Loaded;
        Entry->bComplete = true;
        Entry->Condition.notify_all();
        Impl_->Release(Entry);
        if (Context.ShouldStop() &&
            Loaded.Result == EAssetResult::Success)
            Loaded.Result = EAssetResult::Cancelled;
        return Loaded;
    }

    std::unique_lock Lock(Impl_->Mutex);
    while (!Entry->bComplete && !Context.ShouldStop())
        Entry->Condition.wait_for(Lock, std::chrono::milliseconds(1));
    if (!Entry->bComplete)
    {
        Impl_->Release(Entry);
        FAssetLoadScratchResult Cancelled;
        Cancelled.Result = EAssetResult::Cancelled;
        return Cancelled;
    }
    FAssetLoadScratchResult Loaded = Entry->Result;
    Impl_->Release(Entry);
    if (Context.ShouldStop() && Loaded.Result == EAssetResult::Success)
        Loaded.Result = EAssetResult::Cancelled;
    return Loaded;
}

Core::uint32 FAssetNodeLoadCoordinator::ActiveEntries() const noexcept
{
    std::lock_guard Lock(Impl_->Mutex);
    return static_cast<Core::uint32>(Impl_->Entries.size());
}

} // namespace Stoner::Asset::Private
