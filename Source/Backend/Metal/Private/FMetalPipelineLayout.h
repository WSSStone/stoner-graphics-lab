#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIPipelineLayout.h"

namespace Stoner::Backend::Metal::Private
{

class FMetalPipelineLayout final
    : public RHI::IRHIPipelineLayout,
      public FMetalNativeObject
{
public:
    FMetalPipelineLayout(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::FRHIPipelineLayoutDesc Desc) noexcept;
    ~FMetalPipelineLayout() override;

    [[nodiscard]] const RHI::FRHIPipelineLayoutDesc& GetDesc()
        const noexcept override;
    [[nodiscard]] Core::uint32 GetSetCount() const noexcept override;
    [[nodiscard]] const RHI::FRHIDescriptorBinding* FindBinding(
        Core::uint32 SetIndex,
        Core::uint32 BindingSlot) const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;

private:
    RHI::FRHIPipelineLayoutDesc Desc_;
    Core::uint32 SetCount_ = 0;
};

} // namespace Stoner::Backend::Metal::Private
