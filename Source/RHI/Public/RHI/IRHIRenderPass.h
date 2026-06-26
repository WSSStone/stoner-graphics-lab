#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIRenderPassDesc.h"

namespace Stoner::RHI
{

class IRHIRenderPass
{
public:
    virtual ~IRHIRenderPass() = default;

    [[nodiscard]] virtual const FRHIRenderPassDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetAttachmentCount() const noexcept = 0;
    [[nodiscard]] virtual const FRHIRenderPassAttachmentDesc* GetAttachment(Stoner::Core::uint32 AttachmentIndex) const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
