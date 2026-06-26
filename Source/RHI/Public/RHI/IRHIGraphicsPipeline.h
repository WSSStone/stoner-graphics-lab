#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"

namespace Stoner::RHI
{

class IRHIGraphicsPipeline
{
public:
    virtual ~IRHIGraphicsPipeline() = default;

    [[nodiscard]] virtual const FRHIGraphicsPipelineDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
