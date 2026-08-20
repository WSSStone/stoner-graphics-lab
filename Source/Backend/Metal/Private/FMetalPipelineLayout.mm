#include "FMetalPipelineLayout.h"

#include <algorithm>

namespace Stoner::Backend::Metal::Private
{

FMetalPipelineLayout::FMetalPipelineLayout(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::FRHIPipelineLayoutDesc Desc) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Pipeline),
      Desc_(std::move(Desc))
{
    for (const auto& Binding : Desc_.Bindings)
        SetCount_ = std::max(SetCount_, Binding.SetIndex + 1);
}

FMetalPipelineLayout::~FMetalPipelineLayout() { (void)Invalidate(); }

const RHI::FRHIPipelineLayoutDesc& FMetalPipelineLayout::GetDesc()
    const noexcept { return Desc_; }
Core::uint32 FMetalPipelineLayout::GetSetCount() const noexcept
{
    return SetCount_;
}
const RHI::FRHIDescriptorBinding* FMetalPipelineLayout::FindBinding(
    Core::uint32 SetIndex, Core::uint32 BindingSlot) const noexcept
{
    return RHI::FindRHIPipelineLayoutBinding(Desc_, SetIndex, BindingSlot);
}
RHI::ERHIResourceLifecycleState FMetalPipelineLayout::GetLifecycleState()
    const noexcept { return GetLifecycle(); }
RHI::ERHIResult FMetalPipelineLayout::Invalidate()
{
    return InvalidateObject();
}

} // namespace Stoner::Backend::Metal::Private
