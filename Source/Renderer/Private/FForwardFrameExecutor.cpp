#include "Renderer/FForwardFrameExecutor.h"

#include "RHI/RHIMinimal.h"

namespace Stoner::Renderer
{

FForwardFrameExecutionResult FForwardFrameExecutor::Execute(
    const FForwardFramePlan& Plan, const FForwardFrameExecutionBindings& Bindings) const
{
    FForwardFrameExecutionResult Out;
    if (!Plan.IsValid() || !Plan.HasRenderableGeometry()) return Out;
    const bool bAggregateDraws = !Bindings.Draws.empty();
    const bool bLegacyGeometry = Bindings.VertexBuffer && Bindings.GraphicsPipeline;
    if (!Bindings.OutputTexture || (!bAggregateDraws && !bLegacyGeometry) || !Bindings.RenderPass ||
        !Bindings.Framebuffer || !Bindings.CommandBuffer ||
        Bindings.OutputTexture->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        !Stoner::RHI::HasRHIFlag(Bindings.OutputTexture->GetUsage(), Stoner::RHI::ERHITextureUsage::ColorAttachment) ||
        (bLegacyGeometry && !Stoner::RHI::HasRHIFlag(
            Bindings.VertexBuffer->GetUsage(), Stoner::RHI::ERHIBufferUsage::Vertex)) ||
        (bAggregateDraws && Bindings.Draws.size() !=
            Plan.AcceptedOpaqueDraws.size() + Plan.AcceptedTransparentDraws.size()))
    {
        Out.Result = EForwardExecutionResult::InvalidBinding;
        return Out;
    }
    for (const auto& Draw : Bindings.Draws)
    {
        if (!Draw.VertexBuffer || !Draw.IndexBuffer || !Draw.Pipeline ||
            Draw.VertexBuffer->GetLifecycleState() !=
                Stoner::RHI::ERHIResourceLifecycleState::Valid ||
            Draw.IndexBuffer->GetLifecycleState() !=
                Stoner::RHI::ERHIResourceLifecycleState::Valid ||
            Draw.Pipeline->GetLifecycleState() !=
                Stoner::RHI::ERHIResourceLifecycleState::Valid ||
            !Stoner::RHI::HasRHIFlag(Draw.VertexBuffer->GetUsage(),
                Stoner::RHI::ERHIBufferUsage::Vertex) ||
            !Stoner::RHI::HasRHIFlag(Draw.IndexBuffer->GetUsage(),
                Stoner::RHI::ERHIBufferUsage::Index) ||
            !Stoner::RHI::IsValidRHIIndexedDrawArguments(Draw.Draw))
        {
            Out.Result = EForwardExecutionResult::InvalidBinding;
            return Out;
        }
        for (const auto& Set : Draw.DescriptorSets)
            if (!Set || Set->GetLifecycleState() !=
                    Stoner::RHI::ERHIResourceLifecycleState::Valid)
            {
                Out.Result = EForwardExecutionResult::InvalidBinding;
                return Out;
            }
    }
    if (Bindings.ReadbackBuffer &&
        (!Stoner::RHI::HasRHIFlag(Bindings.OutputTexture->GetUsage(),
             Stoner::RHI::ERHITextureUsage::CopySource) ||
         !Stoner::RHI::HasRHIFlag(Bindings.ReadbackBuffer->GetUsage(),
             Stoner::RHI::ERHIBufferUsage::CopyDestination)))
    {
        Out.Result = EForwardExecutionResult::InvalidBinding;
        return Out;
    }
    if (Bindings.DepthTexture &&
        (Bindings.DepthTexture->GetLifecycleState() !=
                Stoner::RHI::ERHIResourceLifecycleState::Valid ||
         !Stoner::RHI::HasRHIFlag(Bindings.DepthTexture->GetUsage(),
             Stoner::RHI::ERHITextureUsage::DepthStencilAttachment)))
    {
        Out.Result = EForwardExecutionResult::InvalidBinding;
        return Out;
    }
    for (const auto& Texture : Bindings.AuxiliaryColorTextures)
    {
        if (!Texture || Texture->GetLifecycleState() !=
                Stoner::RHI::ERHIResourceLifecycleState::Valid ||
            !Stoner::RHI::HasRHIFlag(Texture->GetUsage(),
                Stoner::RHI::ERHITextureUsage::ColorAttachment))
        {
            Out.Result = EForwardExecutionResult::InvalidBinding;
            return Out;
        }
    }

    auto& Commands = *Bindings.CommandBuffer;
    Stoner::RHI::FRHIResourceBarrierDesc ToColor;
    ToColor.Texture = Bindings.OutputTexture;
    ToColor.RequiredTextureUsage = Stoner::RHI::ERHITextureUsage::ColorAttachment;
    ToColor.Before = Stoner::RHI::ERHIResourceLayout::Undefined;
    ToColor.After = Stoner::RHI::ERHIResourceLayout::ColorAttachment;
    const Stoner::RHI::FRHIViewport Viewport{0.0f, 0.0f,
        static_cast<float>(Plan.OutputTarget.Extent.Width), static_cast<float>(Plan.OutputTarget.Extent.Height), 0.0f, 1.0f};
    const Stoner::RHI::FRHIScissorRect Scissor{0, 0, Plan.OutputTarget.Extent.Width, Plan.OutputTarget.Extent.Height};
    bool bRecorded = Commands.Begin() == Stoner::RHI::ERHIResult::Success &&
        Commands.RecordLayoutTransition(ToColor) == Stoner::RHI::ERHIResult::Success;
    if (bRecorded && Bindings.DepthTexture)
    {
        Stoner::RHI::FRHIResourceBarrierDesc ToDepth;
        ToDepth.Texture = Bindings.DepthTexture;
        ToDepth.RequiredTextureUsage =
            Stoner::RHI::ERHITextureUsage::DepthStencilAttachment;
        ToDepth.Before = Stoner::RHI::ERHIResourceLayout::Undefined;
        ToDepth.After =
            Stoner::RHI::ERHIResourceLayout::DepthStencilAttachment;
        bRecorded = Commands.RecordLayoutTransition(ToDepth) ==
            Stoner::RHI::ERHIResult::Success;
    }
    for (const auto& Texture : Bindings.AuxiliaryColorTextures)
    {
        if (!bRecorded) break;
        Stoner::RHI::FRHIResourceBarrierDesc ToAuxiliaryColor = ToColor;
        ToAuxiliaryColor.Texture = Texture;
        bRecorded = Commands.RecordLayoutTransition(ToAuxiliaryColor) ==
            Stoner::RHI::ERHIResult::Success;
    }
    bRecorded = bRecorded &&
        Commands.BeginRenderPass(Bindings.RenderPass, Bindings.Framebuffer) == Stoner::RHI::ERHIResult::Success &&
        Commands.SetViewport(Viewport) == Stoner::RHI::ERHIResult::Success &&
        Commands.SetScissor(Scissor) == Stoner::RHI::ERHIResult::Success;
    if (bRecorded && bAggregateDraws)
    {
        for (const auto& Draw : Bindings.Draws)
        {
            bRecorded = Commands.BindGraphicsPipeline(Draw.Pipeline) ==
                    Stoner::RHI::ERHIResult::Success;
            for (const auto& Set : Draw.DescriptorSets)
                bRecorded = bRecorded &&
                    Commands.BindDescriptorSet(Set) ==
                        Stoner::RHI::ERHIResult::Success;
            bRecorded = bRecorded &&
                Commands.BindVertexBuffer(Draw.VertexBuffer) ==
                    Stoner::RHI::ERHIResult::Success &&
                Commands.BindIndexBuffer(Draw.IndexBuffer, Draw.IndexType) ==
                    Stoner::RHI::ERHIResult::Success &&
                Commands.RecordDrawIndexed(Draw.Draw) ==
                    Stoner::RHI::ERHIResult::Success;
            if (!bRecorded) break;
        }
    }
    else if (bRecorded)
    {
        bRecorded = Commands.BindGraphicsPipeline(Bindings.GraphicsPipeline) ==
                Stoner::RHI::ERHIResult::Success &&
            Commands.BindVertexBuffer(Bindings.VertexBuffer) ==
                Stoner::RHI::ERHIResult::Success &&
            Commands.RecordDraw(3, 1) == Stoner::RHI::ERHIResult::Success;
    }
    bRecorded = bRecorded &&
        Commands.EndRenderPass() == Stoner::RHI::ERHIResult::Success;
    if (bRecorded && Bindings.ReadbackBuffer)
    {
        Stoner::RHI::FRHIResourceBarrierDesc ToCopy = ToColor;
        ToCopy.Before = Stoner::RHI::ERHIResourceLayout::ColorAttachment;
        ToCopy.After = Stoner::RHI::ERHIResourceLayout::CopySource;
        ToCopy.RequiredTextureUsage = Stoner::RHI::ERHITextureUsage::CopySource;
        bRecorded = Commands.RecordLayoutTransition(ToCopy) ==
                Stoner::RHI::ERHIResult::Success &&
            Commands.RecordTextureToBufferCopy(
                Bindings.OutputTexture, Bindings.ReadbackBuffer,
                Bindings.ReadbackRegion) == Stoner::RHI::ERHIResult::Success;
    }
    else if (bRecorded && Bindings.bTransitionToPresent)
    {
        Stoner::RHI::FRHIResourceBarrierDesc ToPresent = ToColor;
        ToPresent.Before = Stoner::RHI::ERHIResourceLayout::ColorAttachment;
        ToPresent.After = Stoner::RHI::ERHIResourceLayout::Present;
        bRecorded = Commands.RecordLayoutTransition(ToPresent) ==
            Stoner::RHI::ERHIResult::Success;
    }
    bRecorded = bRecorded && Commands.End() == Stoner::RHI::ERHIResult::Success;
    if (!bRecorded)
    {
        Out.Result = EForwardExecutionResult::RecordFailed;
        Out.RecordedCommandCount = Commands.GetRecordedCommandCount();
        return Out;
    }
    Out.Result = EForwardExecutionResult::Success;
    Out.RecordedDrawCount = bAggregateDraws
        ? static_cast<Stoner::Core::uint32>(Bindings.Draws.size()) : 1;
    Out.RecordedCommandCount = Commands.GetRecordedCommandCount();
    return Out;
}

} // namespace Stoner::Renderer
