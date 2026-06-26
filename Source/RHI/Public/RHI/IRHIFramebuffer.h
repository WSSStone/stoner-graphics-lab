#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIFramebufferDesc.h"

namespace Stoner::RHI
{

class IRHIFramebuffer
{
public:
    virtual ~IRHIFramebuffer() = default;

    [[nodiscard]] virtual const FRHIFramebufferDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::TSharedPtr<IRHIRenderPass> GetRenderPass() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetWidth() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetHeight() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetAttachmentCount() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
