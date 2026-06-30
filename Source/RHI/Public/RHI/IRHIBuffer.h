#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIBufferDesc.h"

namespace Stoner::RHI
{

class IRHIBuffer
{
public:
    virtual ~IRHIBuffer() = default;

    [[nodiscard]] virtual const FRHIBufferDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint64 GetSizeInBytes() const noexcept = 0;
    [[nodiscard]] virtual ERHIBufferUsage GetUsage() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
