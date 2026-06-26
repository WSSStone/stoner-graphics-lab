#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIPipelineLayoutDesc.h"

namespace Stoner::RHI
{

class IRHIPipelineLayout
{
public:
    virtual ~IRHIPipelineLayout() = default;

    [[nodiscard]] virtual const FRHIPipelineLayoutDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetSetCount() const noexcept = 0;
    [[nodiscard]] virtual const FRHIDescriptorBinding* FindBinding(Stoner::Core::uint32 SetIndex, Stoner::Core::uint32 BindingSlot) const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
