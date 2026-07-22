#pragma once

#include "Renderer/FForwardFramePlan.h"

namespace Stoner::RHI
{
class IRHIBuffer;
class IRHICommandBuffer;
class IRHIFramebuffer;
class IRHIGraphicsPipeline;
class IRHIRenderPass;
class IRHITexture;
}

namespace Stoner::Renderer
{

enum class EForwardExecutionResult
{
    Success,
    InvalidPlan,
    InvalidBinding,
    RecordFailed
};

struct FForwardFrameExecutionBindings
{
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> OutputTexture;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> VertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline> GraphicsPipeline;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass> RenderPass;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer> Framebuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer> CommandBuffer;
};

struct FForwardFrameExecutionResult
{
    EForwardExecutionResult Result = EForwardExecutionResult::InvalidPlan;
    Stoner::Core::uint32 RecordedDrawCount = 0;
    Stoner::Core::uint32 RecordedCommandCount = 0;

    [[nodiscard]] bool Succeeded() const noexcept { return Result == EForwardExecutionResult::Success; }
};

class FForwardFrameExecutor
{
public:
    [[nodiscard]] FForwardFrameExecutionResult Execute(
        const FForwardFramePlan& Plan, const FForwardFrameExecutionBindings& Bindings) const;
};

} // namespace Stoner::Renderer
