#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIComputePipelineDesc.h"

namespace Stoner::RHI
{

class IRHIComputePipeline
{
public:
    virtual ~IRHIComputePipeline() = default;

    [[nodiscard]] virtual const FRHIComputePipelineDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
