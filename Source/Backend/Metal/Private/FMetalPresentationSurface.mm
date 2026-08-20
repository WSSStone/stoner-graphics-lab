#include "FMetalPresentationSurface.h"

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
