#include "FMetalSynchronization.h"

#import <Metal/Metal.h>

#include <chrono>

namespace Stoner::Backend::Metal::Private
{

FMetalFence::FMetalFence(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    void* RetainedEvent,
    bool bInitiallySignaled) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Synchronization),
      Event_(RetainedEvent),
      State_(bInitiallySignaled
          ? RHI::ERHIFenceState::Signaled
          : RHI::ERHIFenceState::Unsignaled),
      Epoch_(bInitiallySignaled ? 1 : 0)
{
    id<MTLSharedEvent> Event = (__bridge id<MTLSharedEvent>)Event_;
    if (Event) Event.signaledValue = Epoch_;
}

FMetalFence::~FMetalFence()
{
    (void)InvalidateObject();
    Condition_.notify_all();
    if (Event_) (void)(__bridge_transfer id<MTLSharedEvent>)Event_;
    Event_ = nullptr;
}

RHI::ERHIFenceState FMetalFence::GetState() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return State_;
}

bool FMetalFence::IsSignaled() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return State_ == RHI::ERHIFenceState::Signaled ||
        State_ == RHI::ERHIFenceState::Waited;
}

RHI::ERHIResult FMetalFence::Wait(Core::uint64 TimeoutMicroseconds)
{
    std::unique_lock Lock(Mutex_);
    if (!IsCompatible(GetOwner()))
        return RHI::ERHIResult::InvalidState;
    const auto Ready = [this] {
        return State_ == RHI::ERHIFenceState::Signaled ||
            State_ == RHI::ERHIFenceState::Waited ||
            !IsCompatible(GetOwner());
    };
    if (!Ready())
    {
        if (TimeoutMicroseconds == 0) return RHI::ERHIResult::NotReady;
        if (!Condition_.wait_for(
                Lock, std::chrono::microseconds(TimeoutMicroseconds), Ready))
            return RHI::ERHIResult::Timeout;
    }
    if (!IsCompatible(GetOwner()))
        return RHI::ERHIResult::InvalidState;
    State_ = RHI::ERHIFenceState::Waited;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalFence::Reset()
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) ||
        PendingEpoch_ != 0)
        return RHI::ERHIResult::InvalidState;
    State_ = RHI::ERHIFenceState::Unsignaled;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalFence::Signal()
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) ||
        PendingEpoch_ != 0 || State_ == RHI::ERHIFenceState::Signaled)
        return RHI::ERHIResult::InvalidState;
    ++Epoch_;
    id<MTLSharedEvent> Event = (__bridge id<MTLSharedEvent>)Event_;
    if (Event) Event.signaledValue = Epoch_;
    State_ = RHI::ERHIFenceState::Signaled;
    Condition_.notify_all();
    return RHI::ERHIResult::Success;
}

bool FMetalFence::CanSignalForSubmission(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept
{
    std::lock_guard Lock(Mutex_);
    return IsCompatible(Owner) && PendingEpoch_ == 0 &&
        State_ == RHI::ERHIFenceState::Unsignaled;
}

Core::uint64 FMetalFence::ReserveSubmissionSignal() noexcept
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) || PendingEpoch_ != 0 ||
        State_ != RHI::ERHIFenceState::Unsignaled)
        return 0;
    PendingEpoch_ = Epoch_ + 1;
    return PendingEpoch_;
}

void FMetalFence::EncodeSubmissionSignal(
    void* NativeCommandBuffer, Core::uint64 Epoch) noexcept
{
    id<MTLCommandBuffer> CommandBuffer =
        (__bridge id<MTLCommandBuffer>)NativeCommandBuffer;
    id<MTLSharedEvent> Event = (__bridge id<MTLSharedEvent>)Event_;
    if (CommandBuffer && Event && Epoch != 0)
        [CommandBuffer encodeSignalEvent:Event value:Epoch];
}

void FMetalFence::CompleteSubmissionSignal(
    Core::uint64 Epoch, bool bSucceeded) noexcept
{
    std::lock_guard Lock(Mutex_);
    if (PendingEpoch_ != Epoch) return;
    PendingEpoch_ = 0;
    if (bSucceeded)
    {
        Epoch_ = Epoch;
        State_ = RHI::ERHIFenceState::Signaled;
    }
    Condition_.notify_all();
}

FMetalSemaphore::FMetalSemaphore(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    void* RetainedEvent) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Synchronization),
      Event_(RetainedEvent)
{
}

FMetalSemaphore::~FMetalSemaphore()
{
    (void)InvalidateObject();
    if (Event_) (void)(__bridge_transfer id<MTLSharedEvent>)Event_;
    Event_ = nullptr;
}

RHI::ERHISemaphoreState FMetalSemaphore::GetState() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return State_;
}

bool FMetalSemaphore::IsSignaled() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return State_ == RHI::ERHISemaphoreState::Signaled;
}

RHI::ERHIResult FMetalSemaphore::Signal()
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) ||
        PendingEpoch_ != 0 || State_ == RHI::ERHISemaphoreState::Signaled)
        return RHI::ERHIResult::InvalidState;
    ++Epoch_;
    id<MTLSharedEvent> Event = (__bridge id<MTLSharedEvent>)Event_;
    if (Event) Event.signaledValue = Epoch_;
    State_ = RHI::ERHISemaphoreState::Signaled;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalSemaphore::Consume()
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()))
        return RHI::ERHIResult::InvalidState;
    if (State_ != RHI::ERHISemaphoreState::Signaled)
        return RHI::ERHIResult::NotReady;
    State_ = RHI::ERHISemaphoreState::Consumed;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalSemaphore::Reset()
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) ||
        PendingEpoch_ != 0)
        return RHI::ERHIResult::InvalidState;
    State_ = RHI::ERHISemaphoreState::Unsignaled;
    return RHI::ERHIResult::Success;
}

bool FMetalSemaphore::CanWaitForSubmission(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept
{
    std::lock_guard Lock(Mutex_);
    return IsCompatible(Owner) && PendingWaitEpoch_ == 0 &&
        (State_ == RHI::ERHISemaphoreState::Signaled || PendingEpoch_ != 0);
}

bool FMetalSemaphore::CanSignalForSubmission(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept
{
    std::lock_guard Lock(Mutex_);
    return IsCompatible(Owner) && PendingEpoch_ == 0 &&
        State_ != RHI::ERHISemaphoreState::Signaled;
}

Core::uint64 FMetalSemaphore::GetWaitEpoch() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return Epoch_;
}

Core::uint64 FMetalSemaphore::ReserveSubmissionWait() noexcept
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) || PendingWaitEpoch_ != 0 ||
        (State_ != RHI::ERHISemaphoreState::Signaled && PendingEpoch_ == 0))
        return 0;
    PendingWaitEpoch_ = PendingEpoch_ != 0 ? PendingEpoch_ : Epoch_;
    return PendingWaitEpoch_;
}

void FMetalSemaphore::CancelSubmissionWait(Core::uint64 Epoch) noexcept
{
    std::lock_guard Lock(Mutex_);
    if (PendingWaitEpoch_ == Epoch) PendingWaitEpoch_ = 0;
}

Core::uint64 FMetalSemaphore::ReserveSubmissionSignal() noexcept
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) || PendingEpoch_ != 0 ||
        State_ == RHI::ERHISemaphoreState::Signaled)
        return 0;
    PendingEpoch_ = Epoch_ + 1;
    return PendingEpoch_;
}

void FMetalSemaphore::EncodeSubmissionWait(
    void* NativeCommandBuffer, Core::uint64 Epoch) noexcept
{
    id<MTLCommandBuffer> CommandBuffer =
        (__bridge id<MTLCommandBuffer>)NativeCommandBuffer;
    id<MTLSharedEvent> Event = (__bridge id<MTLSharedEvent>)Event_;
    if (CommandBuffer && Event && Epoch != 0)
        [CommandBuffer encodeWaitForEvent:Event value:Epoch];
}

void FMetalSemaphore::EncodeSubmissionSignal(
    void* NativeCommandBuffer, Core::uint64 Epoch) noexcept
{
    id<MTLCommandBuffer> CommandBuffer =
        (__bridge id<MTLCommandBuffer>)NativeCommandBuffer;
    id<MTLSharedEvent> Event = (__bridge id<MTLSharedEvent>)Event_;
    if (CommandBuffer && Event && Epoch != 0)
        [CommandBuffer encodeSignalEvent:Event value:Epoch];
}

void FMetalSemaphore::CommitSubmissionWait(Core::uint64 Epoch) noexcept
{
    std::lock_guard Lock(Mutex_);
    if (PendingWaitEpoch_ != Epoch) return;
    PendingWaitEpoch_ = 0;
    ConsumedEpoch_ = Epoch;
    State_ = RHI::ERHISemaphoreState::Consumed;
}

void FMetalSemaphore::CompleteSubmissionSignal(
    Core::uint64 Epoch, bool bSucceeded) noexcept
{
    std::lock_guard Lock(Mutex_);
    if (PendingEpoch_ != Epoch) return;
    PendingEpoch_ = 0;
    if (bSucceeded)
    {
        Epoch_ = Epoch;
        State_ = ConsumedEpoch_ == Epoch
            ? RHI::ERHISemaphoreState::Consumed
            : RHI::ERHISemaphoreState::Signaled;
    }
}

} // namespace Stoner::Backend::Metal::Private
