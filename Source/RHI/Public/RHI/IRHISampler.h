#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHISamplerDesc.h"

namespace Stoner::RHI
{

class IRHISampler
{
public:
    virtual ~IRHISampler() = default;

    [[nodiscard]] virtual const FRHISamplerDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
