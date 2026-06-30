#pragma once

#include "RHI/IRHIRenderPass.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanRenderPass final : public Stoner::RHI::IRHIRenderPass
{
public:
    explicit FVulkanRenderPass(Stoner::RHI::FRHIRenderPassDesc InDesc);

    [[nodiscard]] static bool IsSupportedDesc(const Stoner::RHI::FRHIRenderPassDesc& Desc) noexcept;
    [[nodiscard]] const Stoner::RHI::FRHIRenderPassDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetAttachmentCount() const noexcept override;
    [[nodiscard]] const Stoner::RHI::FRHIRenderPassAttachmentDesc* GetAttachment(Stoner::Core::uint32 AttachmentIndex) const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    Stoner::RHI::FRHIRenderPassDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
