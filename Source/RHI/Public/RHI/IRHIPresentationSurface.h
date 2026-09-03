#pragma once

#include "RHI/FRHIPresentationSurfaceDesc.h"
#include "RHI/FRHIPresentationCapabilities.h"
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
    virtual ERHIResult QueryCapabilities(
        FRHIPresentationCapabilities&) const
    {
        return ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual Stoner::Core::uint64
    GetCapabilityGeneration() const noexcept
    {
        return 0;
    }
    virtual ERHIResult NotifyPresentationEnvironmentChanged()
    {
        return ERHIResult::Unsupported;
    }
};

} // namespace Stoner::RHI
