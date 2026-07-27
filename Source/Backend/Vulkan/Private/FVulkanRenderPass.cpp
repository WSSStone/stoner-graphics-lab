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
    return Stoner::RHI::IsValidRHIRenderPassDesc(Desc);
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
