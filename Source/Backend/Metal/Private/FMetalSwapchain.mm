#include "FMetalSwapchain.h"

#include "FMetalSynchronization.h"
#include "FMetalFailureInjector.h"

#include <new>

namespace Stoner::Backend::Metal::Private
{

FMetalSwapchain::FMetalSwapchain(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    Core::TSharedPtr<FMetalPresentationSurface> Surface,
    RHI::FRHISwapchainDesc Desc) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Presentation),
      Surface_(std::move(Surface)),
      Desc_(std::move(Desc))
{
    try
    {
        Images_.resize(Desc_.FramesInFlight);
    }
    catch (const std::bad_alloc&)
    {
        State_ = RHI::ERHISwapchainState::Unavailable;
    }
}

FMetalSwapchain::~FMetalSwapchain()
{
    std::lock_guard Lock(Mutex_);
    if (Surface_ && Surface_->GetContext() && AcquiredGeneration_ != 0)
        Surface_->GetContext()->CancelAcquire(
            CurrentFrameIndex_, AcquiredGeneration_);
    Images_.clear();
    (void)InvalidateObject();
}

RHI::ERHISwapchainState FMetalSwapchain::GetState() const noexcept
{
    std::lock_guard Lock(Mutex_);
    if (!Surface_ || !Surface_->IsValid() ||
        GetLifecycle() != RHI::ERHIResourceLifecycleState::Valid)
        return RHI::ERHISwapchainState::Unavailable;
    return State_;
}

Core::uint32 FMetalSwapchain::GetFrameCount() const noexcept
{
    return Desc_.FramesInFlight;
}

Core::uint32 FMetalSwapchain::GetCurrentFrameIndex() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return CurrentFrameIndex_;
}

Core::TSharedPtr<RHI::IRHITexture> FMetalSwapchain::GetImage(
    Core::uint32 FrameIndex) const
{
    std::lock_guard Lock(Mutex_);
    return FrameIndex < Images_.size() ? Images_[FrameIndex] : nullptr;
}

Core::uint64 FMetalSwapchain::GetGeneration() const noexcept
{
    return Surface_ && Surface_->GetContext()
        ? Surface_->GetContext()->GetGeneration() : 0;
}

RHI::ERHIResult FMetalSwapchain::AcquireNextFrame(Core::uint32& OutFrameIndex)
{
    std::lock_guard Lock(Mutex_);
    OutFrameIndex = 0;
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::DrawableAcquisition))
    {
        GetOwner()->RecordDiagnostic(
            Core::FString("AcquireDrawable"), Core::FString("swapchain"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::DrawableAcquisition)),
            0, CurrentFrameIndex_, {}, Core::FString("retry"));
        return RHI::ERHIResult::Failed;
    }
    if (!Surface_ || !Surface_->IsValid() ||
        State_ == RHI::ERHISwapchainState::Acquired || Images_.empty())
        return RHI::ERHIResult::InvalidState;
    auto Result = Surface_->GetContext()->Acquire(
        CurrentFrameIndex_, Images_[CurrentFrameIndex_], AcquiredGeneration_);
    if (Result != RHI::ERHIResult::Success)
    {
        Images_[CurrentFrameIndex_].reset();
        AcquiredGeneration_ = 0;
        State_ = Result == RHI::ERHIResult::ResizeRequired
            ? RHI::ERHISwapchainState::ResizeRequired
            : RHI::ERHISwapchainState::Unavailable;
        return Result;
    }
    State_ = RHI::ERHISwapchainState::Acquired;
    OutFrameIndex = CurrentFrameIndex_;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalSwapchain::AcquireNextFrame(
    Core::uint32& OutFrameIndex,
    const Core::TSharedPtr<RHI::IRHISemaphore>& SignalSemaphore)
{
    if (!SignalSemaphore) return AcquireNextFrame(OutFrameIndex);
    if (SignalSemaphore->IsSignaled()) return RHI::ERHIResult::InvalidState;
    const auto Result = AcquireNextFrame(OutFrameIndex);
    if (Result != RHI::ERHIResult::Success) return Result;
    const auto SignalResult = SignalSemaphore->Signal();
    if (SignalResult != RHI::ERHIResult::Success)
    {
        std::lock_guard Lock(Mutex_);
        Surface_->GetContext()->CancelAcquire(
            CurrentFrameIndex_, AcquiredGeneration_);
        Images_[CurrentFrameIndex_].reset();
        AcquiredGeneration_ = 0;
        State_ = RHI::ERHISwapchainState::Unavailable;
    }
    return SignalResult;
}

RHI::ERHIResult FMetalSwapchain::Present(Core::uint32 FrameIndex)
{
    return Present(FrameIndex, nullptr);
}

RHI::ERHIResult FMetalSwapchain::Present(
    Core::uint32 FrameIndex,
    const Core::TSharedPtr<RHI::IRHISemaphore>& WaitSemaphore)
{
    std::lock_guard Lock(Mutex_);
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::Presentation))
    {
        GetOwner()->RecordDiagnostic(
            Core::FString("Present"), Core::FString("swapchain"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::Presentation)),
            0, FrameIndex, {}, Core::FString("recreate"));
        return RHI::ERHIResult::Failed;
    }
    if (!Surface_ || !Surface_->IsValid() ||
        State_ != RHI::ERHISwapchainState::Acquired ||
        FrameIndex != CurrentFrameIndex_)
        return RHI::ERHIResult::InvalidState;
    Core::TSharedPtr<FMetalSemaphore> NativeWait;
    if (WaitSemaphore)
    {
        NativeWait = std::dynamic_pointer_cast<FMetalSemaphore>(WaitSemaphore);
        if (!NativeWait || !NativeWait->IsCompatible(GetOwner()))
            return RHI::ERHIResult::InvalidState;
    }
    const auto Result = Surface_->GetContext()->Present(
        FrameIndex, AcquiredGeneration_, NativeWait);
    if (Result != RHI::ERHIResult::Success)
    {
        if (Result == RHI::ERHIResult::NotReady)
            return Result;
        Surface_->GetContext()->CancelAcquire(
            FrameIndex, AcquiredGeneration_);
        Images_[FrameIndex].reset();
        AcquiredGeneration_ = 0;
        State_ = Result == RHI::ERHIResult::ResizeRequired
            ? RHI::ERHISwapchainState::ResizeRequired
            : RHI::ERHISwapchainState::Unavailable;
        return Result;
    }
    Images_[FrameIndex].reset();
    CurrentFrameIndex_ = (CurrentFrameIndex_ + 1) % Desc_.FramesInFlight;
    AcquiredGeneration_ = 0;
    State_ = RHI::ERHISwapchainState::Ready;
    return RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Metal::Private
