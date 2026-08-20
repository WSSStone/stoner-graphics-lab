#include "FMetalFramebuffer.h"

#include "FMetalRenderPass.h"
#include "FMetalTexture.h"

namespace Stoner::Backend::Metal::Private
{

FMetalFramebuffer::FMetalFramebuffer(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::FRHIFramebufferDesc Desc) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Resource),
      Desc_(std::move(Desc))
{
}

FMetalFramebuffer::~FMetalFramebuffer() { (void)Invalidate(); }

bool FMetalFramebuffer::IsCompatibleDesc(
    const RHI::FRHIFramebufferDesc& Desc,
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) noexcept
{
    const auto RenderPass =
        std::dynamic_pointer_cast<FMetalRenderPass>(Desc.RenderPass);
    if (!RenderPass || !RenderPass->IsCompatibleWith(Owner) ||
        Desc.Width == 0 || Desc.Height == 0 ||
        Desc.Attachments.size() != RenderPass->GetAttachmentCount())
        return false;

    for (Core::usize Index = 0; Index < Desc.Attachments.size(); ++Index)
    {
        const auto& Attachment = Desc.Attachments[Index];
        const auto Texture =
            std::dynamic_pointer_cast<FMetalTexture>(Attachment.Texture);
        const auto* PassAttachment = RenderPass->GetAttachment(
            static_cast<Core::uint32>(Index));
        if (!Texture || !Texture->IsCompatible(Owner) || !PassAttachment ||
            Texture->GetFormat() != PassAttachment->Format)
            return false;

        const auto& TextureDesc = Texture->GetDesc();
        if (Attachment.MipLevel >= TextureDesc.MipLevels ||
            Attachment.ArrayLayer >= TextureDesc.ArrayLayers ||
            Desc.Width != RHI::GetRHIMipExtent(
                TextureDesc.Width, Attachment.MipLevel) ||
            Desc.Height != RHI::GetRHIMipExtent(
                TextureDesc.Height, Attachment.MipLevel) ||
            TextureDesc.SampleCount != PassAttachment->SampleCount)
            return false;

        const bool bColor = PassAttachment->Role ==
            RHI::ERHIAttachmentRole::Color;
        const auto RequiredUsage = bColor
            ? RHI::ERHITextureUsage::ColorAttachment
            : RHI::ERHITextureUsage::DepthStencilAttachment;
        if (!RHI::HasRHIFlag(TextureDesc.Usage, RequiredUsage)) return false;
    }
    return true;
}

const RHI::FRHIFramebufferDesc& FMetalFramebuffer::GetDesc() const noexcept
{
    return Desc_;
}

Core::TSharedPtr<RHI::IRHIRenderPass> FMetalFramebuffer::GetRenderPass()
    const noexcept
{
    return Desc_.RenderPass;
}

Core::uint32 FMetalFramebuffer::GetWidth() const noexcept { return Desc_.Width; }
Core::uint32 FMetalFramebuffer::GetHeight() const noexcept { return Desc_.Height; }
Core::uint32 FMetalFramebuffer::GetAttachmentCount() const noexcept
{
    return static_cast<Core::uint32>(Desc_.Attachments.size());
}

RHI::ERHIResourceLifecycleState FMetalFramebuffer::GetLifecycleState()
    const noexcept
{
    return GetLifecycle();
}

RHI::ERHIResult FMetalFramebuffer::Invalidate()
{
    return InvalidateObject();
}

} // namespace Stoner::Backend::Metal::Private
