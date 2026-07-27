#pragma once

#include "RHI/IRHIFramebuffer.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;

class FVulkanFramebuffer final : public Stoner::RHI::IRHIFramebuffer
{
public:
    ~FVulkanFramebuffer() override = default;
    FVulkanFramebuffer(const FVulkanFramebuffer&) = delete;
    FVulkanFramebuffer& operator=(const FVulkanFramebuffer&) = delete;

    [[nodiscard]] static bool IsSupportedDesc(const Stoner::RHI::FRHIFramebufferDesc& Desc) noexcept;
    [[nodiscard]] const Stoner::RHI::FRHIFramebufferDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass> GetRenderPass() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetWidth() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetHeight() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetAttachmentCount() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;

    explicit FVulkanFramebuffer(Stoner::RHI::FRHIFramebufferDesc InDesc);

    Stoner::RHI::FRHIFramebufferDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
