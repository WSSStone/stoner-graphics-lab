#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIRenderPass.h"

namespace Stoner::Backend::Metal::Private
{

class FMetalRenderPass final
    : public RHI::IRHIRenderPass,
      public FMetalNativeObject
{
public:
    FMetalRenderPass(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::FRHIRenderPassDesc Desc) noexcept;
    ~FMetalRenderPass() override;

    [[nodiscard]] const RHI::FRHIRenderPassDesc& GetDesc()
        const noexcept override;
    [[nodiscard]] Core::uint32 GetAttachmentCount() const noexcept override;
    [[nodiscard]] const RHI::FRHIRenderPassAttachmentDesc* GetAttachment(
        Core::uint32 AttachmentIndex) const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;

    [[nodiscard]] bool IsCompatibleWith(
        const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept;

private:
    RHI::FRHIRenderPassDesc Desc_;
};

} // namespace Stoner::Backend::Metal::Private
