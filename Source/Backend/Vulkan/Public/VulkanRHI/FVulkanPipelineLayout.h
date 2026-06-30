#pragma once

#include "RHI/IRHIPipelineLayout.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanPipelineLayout final : public Stoner::RHI::IRHIPipelineLayout
{
public:
    explicit FVulkanPipelineLayout(const Stoner::RHI::FRHIPipelineLayoutDesc& InDesc);

    [[nodiscard]] const Stoner::RHI::FRHIPipelineLayoutDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetSetCount() const noexcept override;
    [[nodiscard]] const Stoner::RHI::FRHIDescriptorBinding* FindBinding(Stoner::Core::uint32 SetIndex, Stoner::Core::uint32 BindingSlot) const noexcept override;
    [[nodiscard]] const Stoner::Core::TArray<Stoner::RHI::FRHIShaderConstantRange>& GetConstantRanges() const noexcept;
    [[nodiscard]] bool IsCompatibleWithShaderInterface(const Stoner::RHI::FRHIShaderInterfaceMetadata& Metadata) const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    Stoner::RHI::FRHIPipelineLayoutDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
