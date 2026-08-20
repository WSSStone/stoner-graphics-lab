#pragma once

#include "FMetalNativeObject.h"
#include "FMetalPresentationContext.h"
#include "RHI/IRHIPresentationSurface.h"

namespace Stoner::Backend::Metal::Private
{

class FMetalPresentationSurface final
    : public RHI::IRHIPresentationSurface,
      public FMetalNativeObject
{
public:
    FMetalPresentationSurface(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::FRHIPresentationSurfaceDesc Desc,
        Core::TSharedPtr<FMetalPresentationContext> Context) noexcept;
    ~FMetalPresentationSurface() override;

    [[nodiscard]] const RHI::FRHIPresentationSurfaceDesc& GetDesc()
        const noexcept override;
    [[nodiscard]] bool IsValid() const noexcept override;
    RHI::ERHIResult Invalidate() override;
    [[nodiscard]] const Core::TSharedPtr<FMetalPresentationContext>&
    GetContext() const noexcept;

private:
    RHI::FRHIPresentationSurfaceDesc Desc_;
    Core::TSharedPtr<FMetalPresentationContext> Context_;
};

} // namespace Stoner::Backend::Metal::Private
