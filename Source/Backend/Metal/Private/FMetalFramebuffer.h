#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIFramebuffer.h"

namespace Stoner::Backend::Metal::Private
{

class FMetalFramebuffer final
    : public RHI::IRHIFramebuffer,
      public FMetalNativeObject
{
public:
    FMetalFramebuffer(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::FRHIFramebufferDesc Desc) noexcept;
    ~FMetalFramebuffer() override;

    [[nodiscard]] static bool IsCompatibleDesc(
        const RHI::FRHIFramebufferDesc& Desc,
        const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) noexcept;
    [[nodiscard]] const RHI::FRHIFramebufferDesc& GetDesc()
        const noexcept override;
    [[nodiscard]] Core::TSharedPtr<RHI::IRHIRenderPass> GetRenderPass()
        const noexcept override;
    [[nodiscard]] Core::uint32 GetWidth() const noexcept override;
    [[nodiscard]] Core::uint32 GetHeight() const noexcept override;
    [[nodiscard]] Core::uint32 GetAttachmentCount() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;

private:
    RHI::FRHIFramebufferDesc Desc_;
};

} // namespace Stoner::Backend::Metal::Private
