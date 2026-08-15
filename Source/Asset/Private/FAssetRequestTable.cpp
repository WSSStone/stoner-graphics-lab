#include "FAssetRequestTable.h"

#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace Stoner::Asset::Private
{
namespace
{
bool IsTerminal(EAssetRequestState State) noexcept
{
    return State == EAssetRequestState::Ready ||
        State == EAssetRequestState::Failed ||
        State == EAssetRequestState::Cancelled;
}
} // namespace

struct FAssetRequestTable::FImpl
{
    struct FSlot
    {
        Core::uint32 Generation = 0;
        bool Occupied = false;
        EAssetRequestState State = EAssetRequestState::Invalid;
        EAssetResult Result = EAssetResult::NotReady;
        Core::uint64 CompletionSequence = 0;
        Core::TSharedPtr<const FAssetPayload> Payload;
    };

    FImpl(Core::uint64 Lifetime, Core::uint32 Capacity)
        : ManagerLifetime(Lifetime), Slots(Capacity)
    {
    }

    [[nodiscard]] FSlot* Find(FAssetRequestHandle Handle)
    {
        if (!Handle.IsValid() ||
            Handle.GetManagerLifetime() != ManagerLifetime ||
            Handle.GetSlot() >= Slots.size())
            return nullptr;
        FSlot& Slot = Slots[Handle.GetSlot()];
        return Slot.Occupied && Slot.Generation == Handle.GetGeneration()
            ? &Slot
            : nullptr;
    }

    [[nodiscard]] const FSlot* Find(FAssetRequestHandle Handle) const
    {
        return const_cast<FImpl*>(this)->Find(Handle);
    }

    Core::uint64 ManagerLifetime = 0;
    mutable std::mutex Mutex;
    std::vector<FSlot> Slots;
};

FAssetRequestTable::FAssetRequestTable(
    Core::uint64 ManagerLifetime,
    Core::uint32 Capacity)
    : Impl_(Core::MakeUnique<FImpl>(ManagerLifetime, Capacity))
{
}

FAssetRequestTable::~FAssetRequestTable() = default;

bool FAssetRequestTable::Allocate(FAssetRequestHandle& OutHandle)
{
    OutHandle = {};
    if (Impl_->ManagerLifetime == 0) return false;
    std::lock_guard Lock(Impl_->Mutex);
    for (Core::uint32 Index = 0; Index < Impl_->Slots.size(); ++Index)
    {
        auto& Slot = Impl_->Slots[Index];
        if (Slot.Occupied) continue;
        if (Slot.Generation == std::numeric_limits<Core::uint32>::max())
            Slot.Generation = 1;
        else ++Slot.Generation;
        Slot.Occupied = true;
        Slot.State = EAssetRequestState::Accepted;
        Slot.Result = EAssetResult::NotReady;
        Slot.CompletionSequence = 0;
        Slot.Payload.reset();
        OutHandle = FAssetRequestHandle(
            Impl_->ManagerLifetime, Index, Slot.Generation);
        return true;
    }
    return false;
}

bool FAssetRequestTable::Transition(
    FAssetRequestHandle Handle,
    EAssetRequestState Expected,
    EAssetRequestState Next)
{
    if (Next == EAssetRequestState::Invalid || IsTerminal(Next)) return false;
    std::lock_guard Lock(Impl_->Mutex);
    auto* Slot = Impl_->Find(Handle);
    if (!Slot || Slot->State != Expected || IsTerminal(Slot->State)) return false;
    Slot->State = Next;
    return true;
}

bool FAssetRequestTable::CommitTerminal(
    FAssetRequestHandle Handle,
    EAssetRequestState TerminalState,
    EAssetResult Result)
{
    if (!IsTerminal(TerminalState)) return false;
    std::lock_guard Lock(Impl_->Mutex);
    auto* Slot = Impl_->Find(Handle);
    if (!Slot || IsTerminal(Slot->State)) return false;
    Slot->State = TerminalState;
    Slot->Result = Result;
    return true;
}

bool FAssetRequestTable::CommitReady(
    FAssetRequestHandle Handle,
    Core::TSharedPtr<const FAssetPayload> Payload)
{
    if (!Payload) return false;
    std::lock_guard Lock(Impl_->Mutex);
    auto* Slot = Impl_->Find(Handle);
    if (!Slot || IsTerminal(Slot->State)) return false;
    Slot->Payload = std::move(Payload);
    Slot->State = EAssetRequestState::Ready;
    Slot->Result = EAssetResult::Success;
    return true;
}

bool FAssetRequestTable::GetPayload(
    FAssetRequestHandle Handle,
    Core::TSharedPtr<const FAssetPayload>& OutPayload) const
{
    OutPayload.reset();
    std::lock_guard Lock(Impl_->Mutex);
    const auto* Slot = Impl_->Find(Handle);
    if (!Slot || Slot->State != EAssetRequestState::Ready || !Slot->Payload)
        return false;
    OutPayload = Slot->Payload;
    return true;
}

bool FAssetRequestTable::Query(
    FAssetRequestHandle Handle,
    FAssetRequestSnapshot& OutSnapshot) const
{
    OutSnapshot = {};
    std::lock_guard Lock(Impl_->Mutex);
    const auto* Slot = Impl_->Find(Handle);
    if (!Slot) return false;
    OutSnapshot.Handle = Handle;
    OutSnapshot.State = Slot->State;
    OutSnapshot.Result = Slot->Result;
    OutSnapshot.CompletionSequence = Slot->CompletionSequence;
    return true;
}

bool FAssetRequestTable::SetCompletionSequence(
    FAssetRequestHandle Handle,
    Core::uint64 Sequence)
{
    if (Sequence == 0) return false;
    std::lock_guard Lock(Impl_->Mutex);
    auto* Slot = Impl_->Find(Handle);
    if (!Slot || !IsTerminal(Slot->State) || Slot->CompletionSequence != 0)
        return false;
    Slot->CompletionSequence = Sequence;
    return true;
}

bool FAssetRequestTable::Release(FAssetRequestHandle Handle)
{
    std::lock_guard Lock(Impl_->Mutex);
    auto* Slot = Impl_->Find(Handle);
    if (!Slot) return false;
    Slot->Occupied = false;
    Slot->State = EAssetRequestState::Invalid;
    Slot->Result = EAssetResult::NotReady;
    Slot->CompletionSequence = 0;
    Slot->Payload.reset();
    return true;
}

void FAssetRequestTable::DropPayloads()
{
    std::lock_guard Lock(Impl_->Mutex);
    for (auto& Slot : Impl_->Slots) Slot.Payload.reset();
}

} // namespace Stoner::Asset::Private
