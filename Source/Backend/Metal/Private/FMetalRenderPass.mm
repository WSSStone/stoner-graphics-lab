#include "FMetalRenderPass.h"

namespace Stoner::Backend::Metal::Private
{

FMetalRenderPass::FMetalRenderPass(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::FRHIRenderPassDesc Desc) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Resource),
      Desc_(std::move(Desc))
{
}

FMetalRenderPass::~FMetalRenderPass() { (void)Invalidate(); }

const RHI::FRHIRenderPassDesc& FMetalRenderPass::GetDesc() const noexcept
{
    return Desc_;
}

Core::uint32 FMetalRenderPass::GetAttachmentCount() const noexcept
{
    return static_cast<Core::uint32>(Desc_.Attachments.size());
}

const RHI::FRHIRenderPassAttachmentDesc* FMetalRenderPass::GetAttachment(
    Core::uint32 AttachmentIndex) const noexcept
{
    return AttachmentIndex < Desc_.Attachments.size()
        ? &Desc_.Attachments[AttachmentIndex] : nullptr;
}

RHI::ERHIResourceLifecycleState FMetalRenderPass::GetLifecycleState()
    const noexcept
{
    return GetLifecycle();
}

RHI::ERHIResult FMetalRenderPass::Invalidate()
{
    return InvalidateObject();
}

bool FMetalRenderPass::IsCompatibleWith(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept
{
    return IsCompatible(Owner);
}

} // namespace Stoner::Backend::Metal::Private
