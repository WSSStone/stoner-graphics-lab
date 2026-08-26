#include "FAssetRuntimeCache.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace Stoner::Asset::Private
{
namespace
{
bool CheckedAdd(Core::uint64 Left, Core::uint64 Right, Core::uint64& Out)
{
    if (Right > std::numeric_limits<Core::uint64>::max() - Left) return false;
    Out = Left + Right;
    return true;
}
} // namespace

struct FAssetRuntimeCache::FState
{
    struct FEntry
    {
        FAssetLoadKey Key;
        FAssetMetadata Metadata;
        Core::TSharedPtr<const FAssetPayload> Payload;
        Core::uint64 PayloadBytes = 0;
        Core::TArray<FAssetOptionalFallback> OptionalFallbacks;
        FAssetRetentionCounts Retentions;
        Core::TArray<FAssetLoadKey> RequiredDependencies;
    };

    explicit FState(Core::uint64 Maximum)
        : MaximumPayloadBytes(Maximum)
    {
    }

    [[nodiscard]] FEntry* Find(const FAssetLoadKey& Key)
    {
        const auto Found = std::find_if(Entries.begin(), Entries.end(),
            [&Key](const FEntry& Entry) { return Entry.Key == Key; });
        return Found == Entries.end() ? nullptr : &*Found;
    }

    void Sweep()
    {
        bool Removed = true;
        while (Removed)
        {
            Removed = false;
            for (auto Iterator = Entries.begin(); Iterator != Entries.end();
                 ++Iterator)
            {
                if (!Iterator->Retentions.IsUnretained()) continue;
                const auto Dependencies = Iterator->RequiredDependencies;
                PayloadBytes -= Iterator->PayloadBytes;
                Entries.erase(Iterator);
                for (const auto& Dependency : Dependencies)
                {
                    FEntry* Entry = Find(Dependency);
                    if (Entry && Entry->Retentions.RequiredDependencies > 0)
                        --Entry->Retentions.RequiredDependencies;
                }
                Removed = true;
                break;
            }
        }
    }

    void ReleaseExternal(const FAssetLoadKey& Key)
    {
        std::lock_guard Lock(Mutex);
        FEntry* Entry = Find(Key);
        if (Entry && Entry->Retentions.ExternalHandles > 0)
            --Entry->Retentions.ExternalHandles;
        Sweep();
    }

    mutable std::mutex Mutex;
    std::vector<FEntry> Entries;
    Core::uint64 PayloadBytes = 0;
    Core::uint64 MaximumPayloadBytes = 0;
};

FAssetHandleControl::FAssetHandleControl(
    FAssetId Identity,
    FAssetVersion Version,
    Core::TSharedPtr<const FAssetPayload> Payload,
    std::function<void()> Release)
    : Identity_(std::move(Identity)), Version_(std::move(Version)),
      Payload_(std::move(Payload)), Release_(std::move(Release))
{
}

FAssetHandleControl::~FAssetHandleControl()
{
    if (Release_) Release_();
}

const FAssetId& FAssetHandleControl::GetIdentity() const noexcept
{
    return Identity_;
}

const FAssetVersion& FAssetHandleControl::GetVersion() const noexcept
{
    return Version_;
}

const Core::TSharedPtr<const FAssetPayload>&
FAssetHandleControl::GetPayload() const noexcept
{
    return Payload_;
}

FAssetRuntimeCache::FAssetRuntimeCache(Core::uint64 MaximumPayloadBytes)
    : State_(Core::MakeShared<FState>(MaximumPayloadBytes))
{
}

FAssetRuntimeCache::~FAssetRuntimeCache()
{
    ClearManagerOwnership();
}

EAssetResult FAssetRuntimeCache::Publish(
    const FAssetLoadKey& Root,
    const FAssetLoadScratchResult& Loaded,
    Core::uint64 RequestInterests,
    Core::TSharedPtr<const FAssetPayload>& OutRootPayload)
{
    OutRootPayload.reset();
    if (RequestInterests == 0 || Loaded.Result != EAssetResult::Success ||
        Loaded.Metadata.empty() ||
        Loaded.Metadata.size() != Loaded.Payloads.size() ||
        Loaded.Metadata.size() != Loaded.PayloadBytes.size())
        return EAssetResult::InvalidInput;

    std::lock_guard Lock(State_->Mutex);
    Core::uint64 AddedBytes = 0;
    Core::TArray<FAssetLoadKey> InsertedKeys;
    bool bContainsRoot = State_->Find(Root) != nullptr;
    for (Core::usize Index = 0; Index < Loaded.Metadata.size(); ++Index)
    {
        const auto& Metadata = Loaded.Metadata[Index];
        const auto& Payload = Loaded.Payloads[Index];
        if (!Payload || Loaded.PayloadBytes[Index] == 0 ||
            Metadata.Validate() != EAssetResult::Success ||
            Payload->GetAssetType() != Metadata.Id.GetAssetType())
            return EAssetResult::ProcessingFailure;
        FAssetLoadKey Key = Root;
        Key.AssetId = Metadata.Id;
        Key.ExpectedType = Metadata.Id.GetAssetType();
        bContainsRoot = bContainsRoot || Key == Root;
        const auto* Existing = State_->Find(Key);
        if (Existing)
        {
            if (!Existing->Metadata.IsCanonicallyEquivalent(Metadata))
                return EAssetResult::Conflict;
            continue;
        }
        if (!CheckedAdd(AddedBytes, Loaded.PayloadBytes[Index], AddedBytes))
            return EAssetResult::CapacityExceeded;
    }
    if (!bContainsRoot) return EAssetResult::NotFound;
    for (const auto& Metadata : Loaded.Metadata)
    {
        for (const auto& Dependency : Metadata.Dependencies)
        {
            if (Dependency.Strength != EAssetDependencyStrength::Required)
                continue;
            FAssetLoadKey DependencyKey = Root;
            DependencyKey.AssetId = Dependency.TargetId;
            DependencyKey.ExpectedType = Dependency.TargetId.GetAssetType();
            const bool bInLoaded = std::any_of(Loaded.Metadata.begin(),
                Loaded.Metadata.end(),
                [&Dependency](const FAssetMetadata& Candidate)
                {
                    return Candidate.Id == Dependency.TargetId;
                });
            if (!bInLoaded && !State_->Find(DependencyKey))
                return EAssetResult::NotFound;
        }
    }
    Core::uint64 Aggregate = 0;
    if (!CheckedAdd(State_->PayloadBytes, AddedBytes, Aggregate) ||
        Aggregate > State_->MaximumPayloadBytes)
        return EAssetResult::CapacityExceeded;

    for (Core::usize Index = 0; Index < Loaded.Metadata.size(); ++Index)
    {
        const auto& Metadata = Loaded.Metadata[Index];
        FAssetLoadKey Key = Root;
        Key.AssetId = Metadata.Id;
        Key.ExpectedType = Metadata.Id.GetAssetType();
        if (State_->Find(Key)) continue;
        FState::FEntry Entry;
        Entry.Key = Key;
        Entry.Metadata = Metadata;
        Entry.Payload = Loaded.Payloads[Index];
        Entry.PayloadBytes = Loaded.PayloadBytes[Index];
        for (const auto& Fallback : Loaded.OptionalFallbacks)
            if (Fallback.Owner == Metadata.Id)
                Entry.OptionalFallbacks.push_back(Fallback);
        for (const auto& Dependency : Metadata.Dependencies)
        {
            if (Dependency.Strength != EAssetDependencyStrength::Required)
                continue;
            FAssetLoadKey DependencyKey = Root;
            DependencyKey.AssetId = Dependency.TargetId;
            DependencyKey.ExpectedType = Dependency.TargetId.GetAssetType();
            Entry.RequiredDependencies.push_back(std::move(DependencyKey));
        }
        State_->Entries.push_back(std::move(Entry));
        State_->PayloadBytes += Loaded.PayloadBytes[Index];
        InsertedKeys.push_back(Key);
    }
    for (const auto& InsertedKey : InsertedKeys)
    {
        FState::FEntry* Inserted = State_->Find(InsertedKey);
        if (!Inserted) return EAssetResult::ProcessingFailure;
        for (const auto& Dependency : Inserted->RequiredDependencies)
        {
            FState::FEntry* DependencyEntry = State_->Find(Dependency);
            if (DependencyEntry)
                ++DependencyEntry->Retentions.RequiredDependencies;
        }
    }
    FState::FEntry* RootEntry = State_->Find(Root);
    if (!RootEntry ||
        RequestInterests > std::numeric_limits<Core::uint64>::max() -
            RootEntry->Retentions.RequestInterests)
        return EAssetResult::ProcessingFailure;
    RootEntry->Retentions.RequestInterests += RequestInterests;
    OutRootPayload = RootEntry->Payload;
    return EAssetResult::Success;
}

bool FAssetRuntimeCache::AcquireRequest(
    const FAssetLoadKey& Key,
    Core::TSharedPtr<const FAssetPayload>& OutPayload)
{
    OutPayload.reset();
    std::lock_guard Lock(State_->Mutex);
    FState::FEntry* Entry = State_->Find(Key);
    if (!Entry || Entry->Retentions.RequestInterests ==
            std::numeric_limits<Core::uint64>::max())
        return false;
    ++Entry->Retentions.RequestInterests;
    OutPayload = Entry->Payload;
    return true;
}

bool FAssetRuntimeCache::BorrowLoaded(
    const FAssetLoadKey& Key,
    FAssetMetadata& OutMetadata,
    Core::TSharedPtr<const FAssetPayload>& OutPayload,
    Core::uint64& OutPayloadBytes,
    Core::TArray<FAssetOptionalFallback>& OutOptionalFallbacks) const
{
    OutMetadata = {};
    OutPayload.reset();
    OutPayloadBytes = 0;
    OutOptionalFallbacks.clear();
    std::lock_guard Lock(State_->Mutex);
    const FState::FEntry* Entry = State_->Find(Key);
    if (!Entry || !Entry->Payload || Entry->PayloadBytes == 0) return false;
    OutMetadata = Entry->Metadata;
    OutPayload = Entry->Payload;
    OutPayloadBytes = Entry->PayloadBytes;
    OutOptionalFallbacks = Entry->OptionalFallbacks;
    return true;
}

void FAssetRuntimeCache::ReleaseRequest(const FAssetLoadKey& Key)
{
    std::lock_guard Lock(State_->Mutex);
    FState::FEntry* Entry = State_->Find(Key);
    if (Entry && Entry->Retentions.RequestInterests > 0)
        --Entry->Retentions.RequestInterests;
    State_->Sweep();
}

EAssetResult FAssetRuntimeCache::AcquireExternal(
    const FAssetLoadKey& Key,
    Core::TSharedPtr<FAssetHandleControl>& OutControl)
{
    OutControl.reset();
    std::lock_guard Lock(State_->Mutex);
    FState::FEntry* Entry = State_->Find(Key);
    if (!Entry) return EAssetResult::NotFound;
    if (Entry->Retentions.ExternalHandles ==
        std::numeric_limits<Core::uint64>::max())
        return EAssetResult::CapacityExceeded;
    ++Entry->Retentions.ExternalHandles;
    std::weak_ptr<FState> Weak = State_;
    OutControl = Core::MakeShared<FAssetHandleControl>(Entry->Metadata.Id,
        Entry->Metadata.Version, Entry->Payload,
        [Weak, Key]
        {
            if (const auto State = Weak.lock()) State->ReleaseExternal(Key);
        });
    return EAssetResult::Success;
}

FAssetRuntimeCacheSnapshot FAssetRuntimeCache::Inspect() const
{
    std::lock_guard Lock(State_->Mutex);
    FAssetRuntimeCacheSnapshot Result;
    Result.Entries = static_cast<Core::uint32>(State_->Entries.size());
    Result.PayloadBytes = State_->PayloadBytes;
    for (const auto& Entry : State_->Entries)
    {
        Result.ExternalHandles += Entry.Retentions.ExternalHandles;
        Result.RequestInterests += Entry.Retentions.RequestInterests;
        Result.RequiredDependencies +=
            Entry.Retentions.RequiredDependencies;
    }
    return Result;
}

Core::TArray<FAssetRuntimeCacheEntrySnapshot>
FAssetRuntimeCache::InspectEntries() const
{
    std::lock_guard Lock(State_->Mutex);
    Core::TArray<FAssetRuntimeCacheEntrySnapshot> Result;
    Result.reserve(State_->Entries.size());
    for (const auto& Entry : State_->Entries)
        Result.push_back(
            {Entry.Key, Entry.PayloadBytes, Entry.Retentions});
    return Result;
}

void FAssetRuntimeCache::ClearManagerOwnership()
{
    std::lock_guard Lock(State_->Mutex);
    State_->Entries.clear();
    State_->PayloadBytes = 0;
}

} // namespace Stoner::Asset::Private
