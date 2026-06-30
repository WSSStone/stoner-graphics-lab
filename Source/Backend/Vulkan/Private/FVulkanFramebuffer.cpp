#include "VulkanRHI/FVulkanFramebuffer.h"

#include "RHI/IRHIRenderPass.h"
#include "RHI/IRHITexture.h"

namespace Stoner::Backend::Vulkan
{

FVulkanFramebuffer::FVulkanFramebuffer(Stoner::RHI::FRHIFramebufferDesc InDesc)
    : Desc(std::move(InDesc))
{
}

bool FVulkanFramebuffer::IsSupportedDesc(const Stoner::RHI::FRHIFramebufferDesc& Desc) noexcept
{
    if (!Desc.RenderPass || Desc.RenderPass->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Desc.Width == 0 || Desc.Height == 0 || Desc.Attachments.size() != Desc.RenderPass->GetAttachmentCount())
    {
        return false;
    }

    for (Stoner::Core::uint32 Index = 0; Index < Desc.Attachments.size(); ++Index)
    {
        const Stoner::RHI::FRHIFramebufferAttachment& Attachment = Desc.Attachments[Index];
        const Stoner::RHI::FRHIRenderPassAttachmentDesc* RenderAttachment = Desc.RenderPass->GetAttachment(Index);
        if (!Attachment.Texture || !RenderAttachment ||
            Attachment.Texture->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
            Attachment.Texture->GetFormat() != RenderAttachment->Format)
        {
            return false;
        }

        const Stoner::RHI::FRHITextureDesc& TextureDesc = Attachment.Texture->GetDesc();
        if (Attachment.MipLevel >= TextureDesc.MipLevels || Attachment.ArrayLayer >= TextureDesc.ArrayLayers ||
            Desc.Width > TextureDesc.Width || Desc.Height > TextureDesc.Height ||
            TextureDesc.SampleCount != RenderAttachment->SampleCount)
        {
            return false;
        }
        if (RenderAttachment->Role == Stoner::RHI::ERHIAttachmentRole::Color &&
            !Stoner::RHI::HasRHIFlag(TextureDesc.Usage, Stoner::RHI::ERHITextureUsage::ColorAttachment))
        {
            return false;
        }
        if (RenderAttachment->Role != Stoner::RHI::ERHIAttachmentRole::Color &&
            !Stoner::RHI::HasRHIFlag(TextureDesc.Usage, Stoner::RHI::ERHITextureUsage::DepthStencilAttachment))
        {
            return false;
        }
    }
    return true;
}

const Stoner::RHI::FRHIFramebufferDesc& FVulkanFramebuffer::GetDesc() const noexcept { return Desc; }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass> FVulkanFramebuffer::GetRenderPass() const noexcept { return Desc.RenderPass; }
Stoner::Core::uint32 FVulkanFramebuffer::GetWidth() const noexcept { return Desc.Width; }
Stoner::Core::uint32 FVulkanFramebuffer::GetHeight() const noexcept { return Desc.Height; }
Stoner::Core::uint32 FVulkanFramebuffer::GetAttachmentCount() const noexcept { return static_cast<Stoner::Core::uint32>(Desc.Attachments.size()); }
Stoner::RHI::ERHIResourceLifecycleState FVulkanFramebuffer::GetLifecycleState() const noexcept { return LifecycleState; }

Stoner::RHI::ERHIResult FVulkanFramebuffer::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
