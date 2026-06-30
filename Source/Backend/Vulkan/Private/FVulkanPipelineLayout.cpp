#include "VulkanRHI/FVulkanPipelineLayout.h"

namespace Stoner::Backend::Vulkan
{

FVulkanPipelineLayout::FVulkanPipelineLayout(const Stoner::RHI::FRHIPipelineLayoutDesc& InDesc)
    : Desc(InDesc)
{
}

const Stoner::RHI::FRHIPipelineLayoutDesc& FVulkanPipelineLayout::GetDesc() const noexcept
{
    return Desc;
}

Stoner::Core::uint32 FVulkanPipelineLayout::GetSetCount() const noexcept
{
    Stoner::Core::uint32 Count = 0;
    for (const Stoner::RHI::FRHIDescriptorBinding& Binding : Desc.Bindings)
    {
        if (Binding.SetIndex + 1 > Count)
        {
            Count = Binding.SetIndex + 1;
        }
    }
    return Count;
}

const Stoner::RHI::FRHIDescriptorBinding* FVulkanPipelineLayout::FindBinding(Stoner::Core::uint32 SetIndex, Stoner::Core::uint32 BindingSlot) const noexcept
{
    for (const Stoner::RHI::FRHIDescriptorBinding& Binding : Desc.Bindings)
    {
        if (Binding.SetIndex == SetIndex && Binding.BindingSlot == BindingSlot)
        {
            return &Binding;
        }
    }
    return nullptr;
}

Stoner::RHI::ERHIResourceLifecycleState FVulkanPipelineLayout::GetLifecycleState() const noexcept
{
    return LifecycleState;
}

Stoner::RHI::ERHIResult FVulkanPipelineLayout::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
