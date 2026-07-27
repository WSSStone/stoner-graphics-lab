#include "VulkanRHI/FVulkanPipelineLayout.h"

#include "VulkanRHI/FVulkanDeviceOwnerState.h"

namespace Stoner::Backend::Vulkan
{

FVulkanPipelineLayout::FVulkanPipelineLayout(
    const Stoner::RHI::FRHIPipelineLayoutDesc& InDesc,
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner)
    : Desc(InDesc)
    , Owner(std::move(InOwner))
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

const Stoner::Core::TArray<Stoner::RHI::FRHIShaderConstantRange>& FVulkanPipelineLayout::GetConstantRanges() const noexcept
{
    return Desc.ConstantRanges;
}

bool FVulkanPipelineLayout::IsCompatibleWithShaderInterface(const Stoner::RHI::FRHIShaderInterfaceMetadata& Metadata) const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive &&
        Stoner::RHI::IsRHIShaderInterfaceCompatibleWithPipelineLayout(Metadata, Desc);
}

bool FVulkanPipelineLayout::BelongsTo(
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive && InOwner && Owner == InOwner;
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
    Owner.reset();
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
