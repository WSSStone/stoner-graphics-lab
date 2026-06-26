#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIShaderModuleDesc.h"

namespace Stoner::RHI
{

class IRHIShaderModule
{
public:
    virtual ~IRHIShaderModule() = default;

    [[nodiscard]] virtual const FRHIShaderModuleDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual ERHIShaderStage GetStage() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
