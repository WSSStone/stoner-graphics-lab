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
        if (Surface_ && Surface_->GetContext())
            ResolvedState_ =
                Surface_->GetContext()->GetResolvedPresentationState();
        if (!ResolvedState_.IsValid())
            State_ = RHI::ERHISwapchainState::Unavailable;
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

const RHI::FRHIResolvedPresentationState&
FMetalSwapchain::GetResolvedPresentationState() const noexcept
{
    return ResolvedState_;
}

RHI::ERHIResult FMetalSwapchain::Reconfigure(
    const RHI::FRHISwapchainDesc& Request)
{
    std::lock_guard Lock(Mutex_);
    if (!Surface_ || !Surface_->IsValid() ||
        State_ == RHI::ERHISwapchainState::Acquired)
        return State_ == RHI::ERHISwapchainState::Acquired
            ? RHI::ERHIResult::NotReady
            : RHI::ERHIResult::InvalidState;
    if (Request.IsZeroDrawable())
    {
        for (auto& Image : Images_) Image.reset();
        Images_.clear();
        Desc_ = Request;
        ResolvedState_ = {};
        AcquiredGeneration_ = 0;
        AcquiredFrameToken_ = 0;
        CurrentFrameIndex_ = 0;
        State_ = RHI::ERHISwapchainState::Paused;
        return RHI::ERHIResult::NotReady;
    }
    RHI::FRHIPresentationCapabilities Capabilities;
    if (!Request.IsExactPresentationRequestValid() ||
        Surface_->QueryCapabilities(Capabilities) !=
            RHI::ERHIResult::Success ||
        Request.SurfaceCapabilityGeneration !=
            Capabilities.CapabilityGeneration ||
        !Capabilities.SupportsPair(
            Request.PreferredFormat, Request.PreferredColorSpace))
        return RHI::ERHIResult::Unsupported;

    Core::TArray<Core::TSharedPtr<RHI::IRHITexture>> NewImages;
    try
    {
        NewImages.resize(Request.FramesInFlight);
    }
    catch (const std::bad_alloc&)
    {
        return RHI::ERHIResult::Failed;
    }
    const RHI::ERHIResult ReconfigureResult =
        Surface_->GetContext()->Reconfigure(
            Surface_->GetDesc().Window, Request);
    if (ReconfigureResult != RHI::ERHIResult::Success)
        return ReconfigureResult;

    for (auto& Image : Images_) Image.reset();
    Images_ = std::move(NewImages);
    Desc_ = Request;
    CurrentFrameIndex_ = 0;
    AcquiredGeneration_ = 0;
    AcquiredFrameToken_ = 0;
    ResolvedState_ =
        Surface_->GetContext()->GetResolvedPresentationState();
    State_ = ResolvedState_.IsValid()
        ? RHI::ERHISwapchainState::Ready
        : RHI::ERHISwapchainState::Unavailable;
    return State_ == RHI::ERHISwapchainState::Ready
        ? RHI::ERHIResult::Success
        : RHI::ERHIResult::Failed;
}

RHI::ERHIResult FMetalSwapchain::AcquireNextFrame(Core::uint32& OutFrameIndex)
{
    std::lock_guard Lock(Mutex_);
    const Core::uint64 FrameToken = NextFrameToken_++;
    return AcquireNextFrameLocked(FrameToken, OutFrameIndex);
}

RHI::ERHIResult FMetalSwapchain::AcquireNextFrameLocked(
    Core::uint64 FrameToken,
    Core::uint32& OutFrameIndex)
{
    OutFrameIndex = 0;
    if (FrameToken == 0) return RHI::ERHIResult::InvalidState;
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
    if (Surface_->GetCapabilityGeneration() !=
        Desc_.SurfaceCapabilityGeneration)
    {
        State_ = RHI::ERHISwapchainState::ResizeRequired;
        return RHI::ERHIResult::ResizeRequired;
    }
    auto Result = Surface_->GetContext()->Acquire(
        CurrentFrameIndex_, FrameToken, Images_[CurrentFrameIndex_],
        AcquiredGeneration_);
    ResolvedState_ =
        Surface_->GetContext()->GetResolvedPresentationState();
    if (Result != RHI::ERHIResult::Success)
    {
        Images_[CurrentFrameIndex_].reset();
        AcquiredGeneration_ = 0;
        AcquiredFrameToken_ = 0;
        State_ = Result == RHI::ERHIResult::ResizeRequired
            ? RHI::ERHISwapchainState::ResizeRequired
            : RHI::ERHISwapchainState::Unavailable;
        return Result;
    }
    State_ = RHI::ERHISwapchainState::Acquired;
    AcquiredFrameToken_ = FrameToken;
    OutFrameIndex = CurrentFrameIndex_;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalSwapchain::AcquireNextFrame(
    Core::uint64 FrameToken,
    RHI::FRHIPresentationFrame& OutFrame)
{
    OutFrame = {};
    std::lock_guard Lock(Mutex_);
    Core::uint32 ImageIndex = 0;
    const RHI::ERHIResult Result =
        AcquireNextFrameLocked(FrameToken, ImageIndex);
    if (Result != RHI::ERHIResult::Success) return Result;
    OutFrame.FrameToken = FrameToken;
    OutFrame.ModeGeneration = ResolvedState_.ModeGeneration;
    OutFrame.SwapchainImageGeneration =
        ResolvedState_.SwapchainImageGeneration;
    OutFrame.ImageIndex = ImageIndex;
    OutFrame.Width = ResolvedState_.Width;
    OutFrame.Height = ResolvedState_.Height;
    OutFrame.Format = ResolvedState_.Format;
    OutFrame.ColorSpace = ResolvedState_.ColorSpace;
    OutFrame.DisplayAdaptation = ResolvedState_.DisplayAdaptation;
    OutFrame.MetadataDigest = ResolvedState_.MetadataDigest;
    if (OutFrame.IsValid()) return RHI::ERHIResult::Success;
    Surface_->GetContext()->CancelAcquire(
        CurrentFrameIndex_, AcquiredGeneration_);
    Images_[CurrentFrameIndex_].reset();
    AcquiredGeneration_ = 0;
    AcquiredFrameToken_ = 0;
    State_ = RHI::ERHISwapchainState::Unavailable;
    return RHI::ERHIResult::InvalidState;
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
        AcquiredFrameToken_ = 0;
        State_ = RHI::ERHISwapchainState::Unavailable;
    }
    return SignalResult;
}

RHI::ERHIResult FMetalSwapchain::Present(Core::uint32 FrameIndex)
{
    std::lock_guard Lock(Mutex_);
    return PresentLocked(FrameIndex, AcquiredFrameToken_, nullptr);
}

RHI::ERHIResult FMetalSwapchain::Present(
    const RHI::FRHIPresentationFrame& Frame)
{
    std::lock_guard Lock(Mutex_);
    if (!Frame.Matches(ResolvedState_) ||
        Frame.FrameToken != AcquiredFrameToken_)
        return RHI::ERHIResult::InvalidState;
    return PresentLocked(Frame.ImageIndex, Frame.FrameToken, nullptr);
}

RHI::ERHIResult FMetalSwapchain::Present(
    Core::uint32 FrameIndex,
    const Core::TSharedPtr<RHI::IRHISemaphore>& WaitSemaphore)
{
    std::lock_guard Lock(Mutex_);
    return PresentLocked(FrameIndex, AcquiredFrameToken_, WaitSemaphore);
}

RHI::ERHIResult FMetalSwapchain::PresentLocked(
    Core::uint32 FrameIndex,
    Core::uint64 FrameToken,
    const Core::TSharedPtr<RHI::IRHISemaphore>& WaitSemaphore)
{
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
    if (Surface_->GetCapabilityGeneration() !=
        Desc_.SurfaceCapabilityGeneration)
    {
        Surface_->GetContext()->CancelAcquire(
            FrameIndex, AcquiredGeneration_);
        Images_[FrameIndex].reset();
        AcquiredGeneration_ = 0;
        AcquiredFrameToken_ = 0;
        State_ = RHI::ERHISwapchainState::ResizeRequired;
        return RHI::ERHIResult::ResizeRequired;
    }
    Core::TSharedPtr<FMetalSemaphore> NativeWait;
    if (WaitSemaphore)
    {
        NativeWait = std::dynamic_pointer_cast<FMetalSemaphore>(WaitSemaphore);
        if (!NativeWait || !NativeWait->IsCompatible(GetOwner()))
            return RHI::ERHIResult::InvalidState;
    }
    const auto Result = Surface_->GetContext()->Present(
        FrameIndex, AcquiredGeneration_, FrameToken, NativeWait);
    if (Result != RHI::ERHIResult::Success)
    {
        if (Result == RHI::ERHIResult::NotReady)
            return Result;
        Surface_->GetContext()->CancelAcquire(
            FrameIndex, AcquiredGeneration_);
        Images_[FrameIndex].reset();
        AcquiredGeneration_ = 0;
        AcquiredFrameToken_ = 0;
        State_ = Result == RHI::ERHIResult::ResizeRequired
            ? RHI::ERHISwapchainState::ResizeRequired
            : RHI::ERHISwapchainState::Unavailable;
        return Result;
    }
    Images_[FrameIndex].reset();
    CurrentFrameIndex_ = (CurrentFrameIndex_ + 1) % Desc_.FramesInFlight;
    AcquiredGeneration_ = 0;
    AcquiredFrameToken_ = 0;
    State_ = RHI::ERHISwapchainState::Ready;
    return RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Metal::Private
