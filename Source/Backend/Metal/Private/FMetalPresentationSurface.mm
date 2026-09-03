#include "FMetalPresentationSurface.h"

#include <cstdint>

namespace Stoner::Backend::Metal::Private
{

FMetalPresentationSurface::FMetalPresentationSurface(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::FRHIPresentationSurfaceDesc Desc,
    Core::TSharedPtr<FMetalPresentationContext> Context) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Presentation),
      Desc_(std::move(Desc)),
      Context_(std::move(Context))
{
    if (Desc_.SurfaceId == 0 && Desc_.Window.IsValid())
    {
        Desc_.SurfaceId = static_cast<Core::uint64>(
            reinterpret_cast<std::uintptr_t>(
                Desc_.Window.GetNativeHandle()));
    }
    if (Context_ && Desc_.SurfaceId != 0)
    {
        (void)Context_->QueryCapabilities(Desc_, 1, Capabilities_);
    }
}

FMetalPresentationSurface::~FMetalPresentationSurface()
{
    (void)Invalidate();
}

const RHI::FRHIPresentationSurfaceDesc&
FMetalPresentationSurface::GetDesc() const noexcept
{
    return Desc_;
}

bool FMetalPresentationSurface::IsValid() const noexcept
{
    return GetLifecycle() == RHI::ERHIResourceLifecycleState::Valid &&
        Context_ != nullptr;
}

RHI::ERHIResult FMetalPresentationSurface::QueryCapabilities(
    RHI::FRHIPresentationCapabilities& OutCapabilities) const
{
    OutCapabilities = {};
    if (!IsValid()) return RHI::ERHIResult::InvalidState;
    std::lock_guard Lock(CapabilityMutex_);
    if (!Capabilities_.IsValid()) return RHI::ERHIResult::Unavailable;
    OutCapabilities = Capabilities_;
    return RHI::ERHIResult::Success;
}

Core::uint64
FMetalPresentationSurface::GetCapabilityGeneration() const noexcept
{
    if (!IsValid()) return 0;
    std::lock_guard Lock(CapabilityMutex_);
    return Capabilities_.IsValid()
        ? Capabilities_.CapabilityGeneration : 0;
}

RHI::ERHIResult
FMetalPresentationSurface::NotifyPresentationEnvironmentChanged()
{
    if (!IsValid() || !Context_) return RHI::ERHIResult::InvalidState;
    RHI::FRHIPresentationCapabilities Current;
    {
        std::lock_guard Lock(CapabilityMutex_);
        Current = Capabilities_;
    }
    const Core::uint64 NextGeneration =
        Current.CapabilityGeneration == 0
        ? 1 : Current.CapabilityGeneration + 1;
    RHI::FRHIPresentationCapabilities Refreshed;
    const RHI::ERHIResult Result = Context_->QueryCapabilities(
        Desc_, NextGeneration, Refreshed);
    if (Result != RHI::ERHIResult::Success) return Result;
    std::lock_guard Lock(CapabilityMutex_);
    Capabilities_ = std::move(Refreshed);
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalPresentationSurface::Invalidate()
{
    const auto Result = InvalidateObject();
    if (Result != RHI::ERHIResult::Success) return Result;
    if (Context_ && Context_->IsAttached()) return Context_->Shutdown();
    return RHI::ERHIResult::Success;
}

const Core::TSharedPtr<FMetalPresentationContext>&
FMetalPresentationSurface::GetContext() const noexcept
{
    return Context_;
}

} // namespace Stoner::Backend::Metal::Private
