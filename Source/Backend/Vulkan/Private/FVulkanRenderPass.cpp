#include "VulkanRHI/FVulkanRenderPass.h"

#include "RHI/FRHITextureDesc.h"

namespace Stoner::Backend::Vulkan
{

FVulkanRenderPass::FVulkanRenderPass(Stoner::RHI::FRHIRenderPassDesc InDesc)
    : Desc(std::move(InDesc))
{
}

bool FVulkanRenderPass::IsSupportedDesc(const Stoner::RHI::FRHIRenderPassDesc& Desc) noexcept
{
    if (Desc.Attachments.empty())
    {
        return false;
    }

    bool bHasDepthStencil = false;
    for (const Stoner::RHI::FRHIRenderPassAttachmentDesc& Attachment : Desc.Attachments)
    {
        if (Attachment.Format == Stoner::RHI::ERHIFormat::Unknown)
        {
            return false;
        }
        if (Attachment.Role == Stoner::RHI::ERHIAttachmentRole::Color && Stoner::RHI::IsDepthStencilFormat(Attachment.Format))
        {
            return false;
        }
        if (Attachment.Role != Stoner::RHI::ERHIAttachmentRole::Color)
        {
            if (!Stoner::RHI::IsDepthStencilFormat(Attachment.Format) || bHasDepthStencil)
            {
                return false;
            }
            bHasDepthStencil = true;
        }
    }
    return true;
}

const Stoner::RHI::FRHIRenderPassDesc& FVulkanRenderPass::GetDesc() const noexcept { return Desc; }
Stoner::Core::uint32 FVulkanRenderPass::GetAttachmentCount() const noexcept { return static_cast<Stoner::Core::uint32>(Desc.Attachments.size()); }
const Stoner::RHI::FRHIRenderPassAttachmentDesc* FVulkanRenderPass::GetAttachment(Stoner::Core::uint32 AttachmentIndex) const noexcept
{
    return AttachmentIndex < Desc.Attachments.size() ? &Desc.Attachments[AttachmentIndex] : nullptr;
}
Stoner::RHI::ERHIResourceLifecycleState FVulkanRenderPass::GetLifecycleState() const noexcept { return LifecycleState; }

Stoner::RHI::ERHIResult FVulkanRenderPass::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
