#pragma once

#include "RHI/FRHIPresentationSurfaceDesc.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{

class IRHIPresentationSurface
{
public:
    virtual ~IRHIPresentationSurface() = default;
    [[nodiscard]] virtual const FRHIPresentationSurfaceDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual bool IsValid() const noexcept = 0;
    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
