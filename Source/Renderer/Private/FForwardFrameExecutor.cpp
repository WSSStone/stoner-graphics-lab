#include "Renderer/FForwardFrameExecutor.h"

#include "RHI/RHIMinimal.h"

namespace Stoner::Renderer
{

FForwardFrameExecutionResult FForwardFrameExecutor::Execute(
    const FForwardFramePlan& Plan, const FForwardFrameExecutionBindings& Bindings) const
{
    FForwardFrameExecutionResult Out;
    if (!Plan.IsValid() || !Plan.HasRenderableGeometry()) return Out;
    if (!Bindings.OutputTexture || !Bindings.VertexBuffer || !Bindings.GraphicsPipeline || !Bindings.RenderPass ||
        !Bindings.Framebuffer || !Bindings.CommandBuffer ||
        Bindings.OutputTexture->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        !Stoner::RHI::HasRHIFlag(Bindings.OutputTexture->GetUsage(), Stoner::RHI::ERHITextureUsage::ColorAttachment) ||
        !Stoner::RHI::HasRHIFlag(Bindings.VertexBuffer->GetUsage(), Stoner::RHI::ERHIBufferUsage::Vertex))
    {
        Out.Result = EForwardExecutionResult::InvalidBinding;
        return Out;
    }

    auto& Commands = *Bindings.CommandBuffer;
    Stoner::RHI::FRHIResourceBarrierDesc ToColor;
    ToColor.Texture = Bindings.OutputTexture;
    ToColor.RequiredTextureUsage = Stoner::RHI::ERHITextureUsage::ColorAttachment;
    ToColor.Before = Stoner::RHI::ERHIResourceLayout::Undefined;
    ToColor.After = Stoner::RHI::ERHIResourceLayout::ColorAttachment;
    Stoner::RHI::FRHIResourceBarrierDesc ToPresent = ToColor;
    ToPresent.Before = Stoner::RHI::ERHIResourceLayout::ColorAttachment;
    ToPresent.After = Stoner::RHI::ERHIResourceLayout::Present;

    const Stoner::RHI::FRHIViewport Viewport{0.0f, 0.0f,
        static_cast<float>(Plan.OutputTarget.Extent.Width), static_cast<float>(Plan.OutputTarget.Extent.Height), 0.0f, 1.0f};
    const Stoner::RHI::FRHIScissorRect Scissor{0, 0, Plan.OutputTarget.Extent.Width, Plan.OutputTarget.Extent.Height};
    const bool bRecorded =
        Commands.Begin() == Stoner::RHI::ERHIResult::Success &&
        Commands.RecordLayoutTransition(ToColor) == Stoner::RHI::ERHIResult::Success &&
        Commands.BeginRenderPass(Bindings.RenderPass, Bindings.Framebuffer) == Stoner::RHI::ERHIResult::Success &&
        Commands.BindGraphicsPipeline(Bindings.GraphicsPipeline) == Stoner::RHI::ERHIResult::Success &&
        Commands.BindVertexBuffer(Bindings.VertexBuffer) == Stoner::RHI::ERHIResult::Success &&
        Commands.SetViewport(Viewport) == Stoner::RHI::ERHIResult::Success &&
        Commands.SetScissor(Scissor) == Stoner::RHI::ERHIResult::Success &&
        Commands.RecordDraw(3, 1) == Stoner::RHI::ERHIResult::Success &&
        Commands.EndRenderPass() == Stoner::RHI::ERHIResult::Success &&
        Commands.RecordLayoutTransition(ToPresent) == Stoner::RHI::ERHIResult::Success &&
        Commands.End() == Stoner::RHI::ERHIResult::Success;
    if (!bRecorded)
    {
        Out.Result = EForwardExecutionResult::RecordFailed;
        return Out;
    }
    Out.Result = EForwardExecutionResult::Success;
    Out.RecordedDrawCount = 1;
    Out.RecordedCommandCount = Commands.GetRecordedCommandCount();
    return Out;
}

} // namespace Stoner::Renderer
