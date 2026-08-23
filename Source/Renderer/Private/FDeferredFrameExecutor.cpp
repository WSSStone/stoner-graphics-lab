#include "Renderer/FDeferredFrameExecutor.h"
#include "Renderer/FDeferredLightVolume.h"

#include <cmath>

namespace Stoner::Renderer
{

namespace
{

[[nodiscard]] bool IsTextureValid(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture,
    Stoner::RHI::ERHIFormat Format, FDeferredExtent2D Extent,
    Stoner::RHI::ERHITextureUsage Usage) noexcept
{
    return Texture && Texture->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Texture->GetFormat() == Format && Texture->GetDesc().Width == Extent.Width &&
        Texture->GetDesc().Height == Extent.Height &&
        Stoner::RHI::HasRHIFlag(Texture->GetUsage(), Usage);
}

[[nodiscard]] bool IsBufferValid(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer,
    Stoner::RHI::ERHIBufferUsage Usage) noexcept
{
    return Buffer && Buffer->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Stoner::RHI::HasRHIFlag(Buffer->GetUsage(), Usage);
}

[[nodiscard]] bool IsStageValid(const FDeferredStageBindings& Stage) noexcept
{
    if (!Stage.RenderPass || !Stage.Framebuffer || !Stage.Pipeline ||
        Stage.RenderPass->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Stage.Framebuffer->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Stage.Pipeline->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Stage.Framebuffer->GetRenderPass() != Stage.RenderPass)
    {
        return false;
    }
    for (const auto& Set : Stage.DescriptorSets)
    {
        if (!Set || Set->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const FDeferredStageBindings* FindStage(const FDeferredFrameExecutionBindings& Bindings,
    EDeferredPassStage Stage)
{
    switch (Stage)
    {
    case EDeferredPassStage::SurfaceData: return &Bindings.Surface;
    case EDeferredPassStage::DirectionalLighting: return &Bindings.Directional;
    case EDeferredPassStage::PointLightVolumes: return &Bindings.PointOutside;
    case EDeferredPassStage::SpotLightVolumes: return &Bindings.SpotOutside;
    case EDeferredPassStage::Composition: return &Bindings.Composition;
    case EDeferredPassStage::ForwardTransparency: return &Bindings.Transparency;
    case EDeferredPassStage::ValidationReadback: return nullptr;
    }
    return nullptr;
}

Stoner::RHI::FRHIRenderPassClearValues MakeClearValues(const FDeferredFramePlan& Plan,
    EDeferredPassStage Stage, const Stoner::RHI::IRHIRenderPass& RenderPass)
{
    Stoner::RHI::FRHIRenderPassClearValues Values;
    Values.Depth = Plan.SurfaceLayout.DepthPolicy.FarClearValue;
    for (const Stoner::RHI::FRHIRenderPassAttachmentDesc& Attachment : RenderPass.GetDesc().Attachments)
    {
        if (Attachment.Role == Stoner::RHI::ERHIAttachmentRole::Color &&
            Attachment.LoadOp == Stoner::RHI::ERHIAttachmentLoadOp::Clear)
        {
            Values.Colors.push_back(Stage == EDeferredPassStage::SurfaceData
                ? Stoner::RHI::FRHIClearColor{0.0f, 0.0f, 0.0f, 0.0f}
                : Stoner::RHI::FRHIClearColor{0.0f, 0.0f, 0.0f, 1.0f});
        }
    }
    return Values;
}

[[nodiscard]] bool BindDescriptorSets(Stoner::RHI::IRHICommandBuffer& Commands,
    const FDeferredStageBindings& Stage)
{
    for (const auto& DescriptorSet : Stage.DescriptorSets)
    {
        if (Commands.BindDescriptorSet(DescriptorSet) != Stoner::RHI::ERHIResult::Success)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool AreSurfaceDrawsValid(
    const FDeferredFramePlan& Plan,
    const FDeferredFrameExecutionBindings& Bindings) noexcept
{
    if (Bindings.SurfaceDraws.empty()) return true;
    if (Bindings.SurfaceDraws.size() != Plan.AcceptedDraws.size()) return false;
    for (const auto& Draw : Bindings.SurfaceDraws)
    {
        if (!IsBufferValid(Draw.VertexBuffer,
                Stoner::RHI::ERHIBufferUsage::Vertex) ||
            !IsBufferValid(Draw.IndexBuffer,
                Stoner::RHI::ERHIBufferUsage::Index) ||
            !Stoner::RHI::IsValidRHIIndexedDrawArguments(Draw.Draw) ||
            !Draw.Pipeline ||
            Draw.Pipeline->GetLifecycleState() !=
                Stoner::RHI::ERHIResourceLifecycleState::Valid)
            return false;
        for (const auto& Set : Draw.DescriptorSets)
            if (!Set || Set->GetLifecycleState() !=
                    Stoner::RHI::ERHIResourceLifecycleState::Valid)
                return false;
    }
    return true;
}

[[nodiscard]] bool BindDescriptorSets(
    Stoner::RHI::IRHICommandBuffer& Commands,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<
        Stoner::RHI::IRHIDescriptorSet>>& DescriptorSets)
{
    for (const auto& DescriptorSet : DescriptorSets)
        if (Commands.BindDescriptorSet(DescriptorSet) !=
            Stoner::RHI::ERHIResult::Success)
            return false;
    return true;
}

[[nodiscard]] bool TransitionTexture(
    Stoner::RHI::IRHICommandBuffer& Commands,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture,
    Stoner::RHI::ERHITextureUsage RequiredUsage,
    Stoner::RHI::ERHIResourceLayout Before,
    Stoner::RHI::ERHIResourceLayout After)
{
    Stoner::RHI::FRHIResourceBarrierDesc Transition;
    Transition.Texture = Texture;
    Transition.RequiredTextureUsage = RequiredUsage;
    Transition.Before = Before;
    Transition.After = After;
    return Commands.RecordLayoutTransition(Transition) ==
        Stoner::RHI::ERHIResult::Success;
}

} // namespace

FDeferredFrameExecutionResult FDeferredFrameExecutor::Execute(const FDeferredFramePlan& Plan,
    const FDeferredRenderGraphDeclaration& Graph,
    const FDeferredFrameExecutionBindings& Bindings) const
{
    FDeferredFrameExecutionResult Out;
    const FDeferredExtent2D Extent = Plan.SurfaceLayout.Extent;
    const bool bBaseValid = Plan.IsValid() && Graph.bValid && Bindings.CommandBuffer &&
        (Bindings.CommandBuffer->GetState() == Stoner::RHI::ERHICommandBufferState::Idle ||
            Bindings.CommandBuffer->GetState() == Stoner::RHI::ERHICommandBufferState::Resettable) &&
        IsTextureValid(Bindings.BaseColorAO, Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm, Extent,
            Stoner::RHI::ERHITextureUsage::ColorAttachment) &&
        IsTextureValid(Bindings.NormalRoughness, Stoner::RHI::ERHIFormat::R16G16B16A16_Float, Extent,
            Stoner::RHI::ERHITextureUsage::ColorAttachment) &&
        IsTextureValid(Bindings.EmissiveMetallic, Stoner::RHI::ERHIFormat::R16G16B16A16_Float, Extent,
            Stoner::RHI::ERHITextureUsage::ColorAttachment) &&
        IsTextureValid(Bindings.Depth, Stoner::RHI::ERHIFormat::D32_Float, Extent,
            Stoner::RHI::ERHITextureUsage::DepthStencilAttachment) &&
        IsTextureValid(Bindings.LightingAccumulation, Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
            Extent, Stoner::RHI::ERHITextureUsage::ColorAttachment) &&
        IsTextureValid(Bindings.FinalOutput, Plan.Output.Format, Extent,
            Stoner::RHI::ERHITextureUsage::ColorAttachment);
    if (!bBaseValid)
    {
        Out.FinalState = EDeferredExecutionState::Failed;
        Out.Diagnostics.Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::SurfaceData,
            EDeferredResult::InvalidBinding, "DEF-EXEC-BINDINGS", Plan.FrameId,
            "frame plan graph command buffer and surface textures must validate transactionally");
        return Out;
    }

    for (const FDeferredPassRecord& Pass : Plan.Passes)
    {
        if (Pass.Stage != EDeferredPassStage::ValidationReadback)
        {
            const FDeferredStageBindings* Stage = FindStage(Bindings, Pass.Stage);
            if (!Stage || !IsStageValid(*Stage))
            {
                Out.FinalState = EDeferredExecutionState::Failed;
                Out.Diagnostics.Add(EDeferredDiagnosticSeverity::Error, Pass.Stage,
                    EDeferredResult::InvalidBinding, "DEF-EXEC-STAGE", Pass.Name,
                    "required render pass framebuffer pipeline or descriptor set is invalid");
                return Out;
            }
        }
    }
    if ((!Bindings.SurfaceDraws.empty()
            ? !AreSurfaceDrawsValid(Plan, Bindings)
            : !IsBufferValid(Bindings.SurfaceVertexBuffer,
                Stoner::RHI::ERHIBufferUsage::Vertex)) ||
        !IsBufferValid(Bindings.FullscreenVertexBuffer, Stoner::RHI::ERHIBufferUsage::Vertex))
    {
        Out.FinalState = EDeferredExecutionState::Failed;
        return Out;
    }
    Out.FinalState = EDeferredExecutionState::BindingsValidated;

    Stoner::RHI::IRHICommandBuffer& Commands = *Bindings.CommandBuffer;
    if (Commands.Begin() != Stoner::RHI::ERHIResult::Success)
    {
        Out.Result = EDeferredResult::RecordFailed;
        Out.FinalState = EDeferredExecutionState::Failed;
        return Out;
    }
    Out.FinalState = EDeferredExecutionState::Recording;
    const Stoner::RHI::FRHIViewport Viewport{0.0f, 0.0f, static_cast<float>(Extent.Width),
        static_cast<float>(Extent.Height), 0.0f, 1.0f};
    const Stoner::RHI::FRHIScissorRect FullScissor{0, 0, Extent.Width, Extent.Height};

    for (const FDeferredPassRecord& Pass : Plan.Passes)
    {
        if (Pass.Stage == EDeferredPassStage::ValidationReadback)
        {
            for (const FDeferredReadbackBinding& Readback : Bindings.Readbacks)
            {
                Stoner::RHI::FRHIResourceBarrierDesc Transition;
                Transition.Texture = Readback.Source;
                Transition.RequiredTextureUsage = Stoner::RHI::ERHITextureUsage::CopySource;
                Transition.Before = Readback.Source == Bindings.FinalOutput
                    ? Stoner::RHI::ERHIResourceLayout::ColorAttachment
                    : Stoner::RHI::ERHIResourceLayout::ShaderReadOnly;
                Transition.After = Stoner::RHI::ERHIResourceLayout::CopySource;
                if (Commands.RecordLayoutTransition(Transition) != Stoner::RHI::ERHIResult::Success ||
                    Commands.RecordTextureToBufferCopy(Readback.Source, Readback.Destination,
                        Readback.Region) != Stoner::RHI::ERHIResult::Success)
                {
                    Out.RecordedCommandCount = Commands.GetRecordedCommandCount();
                    if (Commands.End() == Stoner::RHI::ERHIResult::Success)
                    {
                        (void)Commands.Reset();
                    }
                    Out.Result = EDeferredResult::ReadbackFailed;
                    Out.FinalState = EDeferredExecutionState::Failed;
                    Out.Diagnostics.Add(EDeferredDiagnosticSeverity::Error, Pass.Stage,
                        EDeferredResult::ReadbackFailed, "DEF-EXEC-READBACK", Readback.Name,
                        "readback recording stopped at the first invalid transition or copy");
                    Out.Diagnostics.Add(EDeferredDiagnosticSeverity::Info, Pass.Stage,
                        EDeferredResult::ReadbackFailed, "DEF-EXEC-CLEANUP", Readback.Name,
                        "partial command recording was closed and reset without later success");
                    return Out;
                }
            }
            ++Out.RecordedPassCount;
            Out.LastCompletedStage = Pass.Stage;
            continue;
        }

        const FDeferredStageBindings& Stage = *FindStage(Bindings, Pass.Stage);
        if (Pass.Stage == EDeferredPassStage::Composition &&
            !TransitionTexture(Commands, Bindings.LightingAccumulation,
                Stoner::RHI::ERHITextureUsage::Sampled,
                Stoner::RHI::ERHIResourceLayout::ColorAttachment,
                Stoner::RHI::ERHIResourceLayout::ShaderReadOnly))
        {
            Out.Result = EDeferredResult::RecordFailed;
            Out.FinalState = EDeferredExecutionState::Failed;
            return Out;
        }
        const auto ClearValues = MakeClearValues(Plan, Pass.Stage, *Stage.RenderPass);
        const bool bLocalVolume = Pass.Stage == EDeferredPassStage::PointLightVolumes ||
            Pass.Stage == EDeferredPassStage::SpotLightVolumes;
        bool bRecorded = Commands.BeginRenderPass(Stage.RenderPass, Stage.Framebuffer, ClearValues) ==
                Stoner::RHI::ERHIResult::Success &&
            Commands.SetViewport(Viewport) == Stoner::RHI::ERHIResult::Success;
        if (bRecorded && !bLocalVolume)
        {
            bRecorded = Commands.BindGraphicsPipeline(Stage.Pipeline) ==
                    Stoner::RHI::ERHIResult::Success &&
                BindDescriptorSets(Commands, Stage) &&
                Commands.SetScissor(FullScissor) == Stoner::RHI::ERHIResult::Success;
        }
        if (bRecorded && Pass.Stage == EDeferredPassStage::SurfaceData)
        {
            if (!Bindings.SurfaceDraws.empty())
            {
                for (const auto& Draw : Bindings.SurfaceDraws)
                {
                    bRecorded = bRecorded &&
                        Commands.BindGraphicsPipeline(Draw.Pipeline) ==
                            Stoner::RHI::ERHIResult::Success &&
                        BindDescriptorSets(Commands, Draw.DescriptorSets) &&
                        Commands.BindVertexBuffer(Draw.VertexBuffer) ==
                            Stoner::RHI::ERHIResult::Success &&
                        Commands.BindIndexBuffer(
                            Draw.IndexBuffer, Draw.IndexType) ==
                            Stoner::RHI::ERHIResult::Success &&
                        Commands.RecordDrawIndexed(Draw.Draw) ==
                            Stoner::RHI::ERHIResult::Success;
                    if (!bRecorded) break;
                }
            }
            else
            {
                bRecorded = Commands.BindVertexBuffer(
                    Bindings.SurfaceVertexBuffer) ==
                    Stoner::RHI::ERHIResult::Success;
                if (bRecorded && Bindings.SurfaceIndexCount > 0)
                {
                    bRecorded = IsBufferValid(Bindings.SurfaceIndexBuffer, Stoner::RHI::ERHIBufferUsage::Index) &&
                        Commands.BindIndexBuffer(Bindings.SurfaceIndexBuffer, Stoner::RHI::ERHIIndexType::UInt16) ==
                            Stoner::RHI::ERHIResult::Success &&
                        Commands.RecordDrawIndexed({
                            Bindings.SurfaceIndexCount,
                            static_cast<Stoner::Core::uint32>(Plan.AcceptedDraws.size()),
                            0,
                            0,
                            0}) ==
                            Stoner::RHI::ERHIResult::Success;
                }
                else if (bRecorded && !Plan.AcceptedDraws.empty())
                {
                    bRecorded = Commands.RecordDraw(3,
                        static_cast<Stoner::Core::uint32>(Plan.AcceptedDraws.size())) ==
                        Stoner::RHI::ERHIResult::Success;
                }
            }
        }
        else if (bRecorded && bLocalVolume)
        {
            struct FLightBatch
            {
                const FDeferredStageBindings* Stage = nullptr;
                Stoner::RHI::FRHIScissorRect Scissor;
                Stoner::Core::uint32 InstanceCount = 0;
                Stoner::Core::uint32 FirstInstance = 0;
            };
            const bool bPoint = Pass.Stage == EDeferredPassStage::PointLightVolumes;
            const auto& Vertex = bPoint ? Bindings.SphereVertexBuffer : Bindings.ConeVertexBuffer;
            const auto& Index = bPoint ? Bindings.SphereIndexBuffer : Bindings.ConeIndexBuffer;
            const Stoner::Core::uint32 IndexCount = bPoint ? Bindings.SphereIndexCount : Bindings.ConeIndexCount;
            bRecorded = IsBufferValid(Vertex, Stoner::RHI::ERHIBufferUsage::Vertex) &&
                IsBufferValid(Index, Stoner::RHI::ERHIBufferUsage::Index) && IndexCount > 0 &&
                Commands.BindVertexBuffer(Vertex) == Stoner::RHI::ERHIResult::Success &&
                Commands.BindIndexBuffer(Index, Stoner::RHI::ERHIIndexType::UInt16) ==
                    Stoner::RHI::ERHIResult::Success;
            Stoner::Core::TArray<FLightBatch> Batches;
            for (std::size_t LightIndex = 0; LightIndex < Plan.Lights.Accepted.size();
                ++LightIndex)
            {
                const FDeferredLightRecord& Light = Plan.Lights.Accepted[LightIndex];
                if (Light.Type != (bPoint ? EDeferredLightType::Point : EDeferredLightType::Spot))
                {
                    continue;
                }
                const FDeferredLightVolumeClassification Classification =
                    ClassifyDeferredLightVolume(Light, Plan.View);
                if (!Classification.bIntersectsView ||
                    Classification.Acceptance == EDeferredLightAcceptance::CulledOutsideView)
                {
                    ++Out.OmittedLocalLightCount;
                    continue;
                }
                const bool bInside =
                    Classification.Acceptance == EDeferredLightAcceptance::AcceptedVolumeCameraInside ||
                    Classification.Acceptance ==
                        EDeferredLightAcceptance::AcceptedVolumeNearPlaneIntersection;
                const FDeferredStageBindings& LightStage = bPoint
                    ? (bInside ? Bindings.PointInside : Bindings.PointOutside)
                    : (bInside ? Bindings.SpotInside : Bindings.SpotOutside);
                const Stoner::RHI::FRHIScissorRect Scissor{
                    Classification.Scissor.X, Classification.Scissor.Y,
                    Classification.Scissor.Width, Classification.Scissor.Height};
                const bool bCanAppend = !Batches.empty() && Batches.back().Stage == &LightStage &&
                    Batches.back().Scissor.X == Scissor.X &&
                    Batches.back().Scissor.Y == Scissor.Y &&
                    Batches.back().Scissor.Width == Scissor.Width &&
                    Batches.back().Scissor.Height == Scissor.Height;
                if (bCanAppend)
                {
                    ++Batches.back().InstanceCount;
                }
                else
                {
                    Batches.push_back({&LightStage, Scissor, 1,
                        static_cast<Stoner::Core::uint32>(LightIndex)});
                }
            }
            for (const FLightBatch& Batch : Batches)
            {
                bRecorded = bRecorded && IsStageValid(*Batch.Stage) &&
                    Batch.Stage->RenderPass == Stage.RenderPass &&
                    Batch.Stage->Framebuffer == Stage.Framebuffer &&
                    Commands.BindGraphicsPipeline(Batch.Stage->Pipeline) ==
                        Stoner::RHI::ERHIResult::Success &&
                    BindDescriptorSets(Commands, *Batch.Stage) &&
                    Commands.SetScissor(Batch.Scissor) == Stoner::RHI::ERHIResult::Success &&
                    Commands.RecordDrawIndexed({
                        IndexCount,
                        Batch.InstanceCount,
                        0,
                        0,
                        Batch.FirstInstance}) ==
                        Stoner::RHI::ERHIResult::Success;
                if (!bRecorded)
                {
                    break;
                }
                ++Out.LocalLightBatchCount;
                Out.LocalLightInstanceCount += Batch.InstanceCount;
            }
        }
        else if (bRecorded)
        {
            bRecorded = Commands.BindVertexBuffer(Bindings.FullscreenVertexBuffer) ==
                    Stoner::RHI::ERHIResult::Success &&
                Commands.RecordDraw(3, Pass.DrawCount > 0 ? Pass.DrawCount : 1) ==
                    Stoner::RHI::ERHIResult::Success;
        }
        bRecorded = bRecorded && Commands.EndRenderPass() == Stoner::RHI::ERHIResult::Success;
        if (bRecorded && Pass.Stage == EDeferredPassStage::SurfaceData)
        {
            bRecorded =
                TransitionTexture(Commands, Bindings.BaseColorAO,
                    Stoner::RHI::ERHITextureUsage::Sampled,
                    Stoner::RHI::ERHIResourceLayout::ColorAttachment,
                    Stoner::RHI::ERHIResourceLayout::ShaderReadOnly) &&
                TransitionTexture(Commands, Bindings.NormalRoughness,
                    Stoner::RHI::ERHITextureUsage::Sampled,
                    Stoner::RHI::ERHIResourceLayout::ColorAttachment,
                    Stoner::RHI::ERHIResourceLayout::ShaderReadOnly) &&
                TransitionTexture(Commands, Bindings.EmissiveMetallic,
                    Stoner::RHI::ERHITextureUsage::Sampled,
                    Stoner::RHI::ERHIResourceLayout::ColorAttachment,
                    Stoner::RHI::ERHIResourceLayout::ShaderReadOnly) &&
                TransitionTexture(Commands, Bindings.Depth,
                    Stoner::RHI::ERHITextureUsage::Sampled,
                    Stoner::RHI::ERHIResourceLayout::DepthStencilAttachment,
                    Stoner::RHI::ERHIResourceLayout::ShaderReadOnly);
        }
        if (!bRecorded)
        {
            Out.RecordedCommandCount = Commands.GetRecordedCommandCount();
            (void)Commands.EndRenderPass();
            if (Commands.End() == Stoner::RHI::ERHIResult::Success)
            {
                (void)Commands.Reset();
            }
            Out.Result = EDeferredResult::RecordFailed;
            Out.FinalState = EDeferredExecutionState::Failed;
            Out.Diagnostics.Add(EDeferredDiagnosticSeverity::Error, Pass.Stage,
                EDeferredResult::RecordFailed, "DEF-EXEC-RECORD", Pass.Name,
                "command recording stopped at first failed stage");
            Out.Diagnostics.Add(EDeferredDiagnosticSeverity::Info, Pass.Stage,
                EDeferredResult::RecordFailed, "DEF-EXEC-CLEANUP", Pass.Name,
                "partial command recording was closed and reset without dependent stages");
            return Out;
        }
        ++Out.RecordedPassCount;
        Out.RecordedDrawCount += Pass.DrawCount;
        Out.LastCompletedStage = Pass.Stage;
    }
    if (Commands.End() != Stoner::RHI::ERHIResult::Success)
    {
        Out.RecordedCommandCount = Commands.GetRecordedCommandCount();
        Out.Result = EDeferredResult::RecordFailed;
        Out.FinalState = EDeferredExecutionState::Failed;
        Out.Diagnostics.Add(EDeferredDiagnosticSeverity::Error, Out.LastCompletedStage,
            EDeferredResult::RecordFailed, "DEF-EXEC-END", Plan.FrameId,
            "command recording finalization failed after the last completed stage");
        return Out;
    }
    Out.Result = EDeferredResult::Success;
    Out.FinalState = EDeferredExecutionState::Recorded;
    Out.RecordedCommandCount = Commands.GetRecordedCommandCount();
    return Out;
}

Stoner::Core::TArray<FDeferredShaderBindingContract> GetCanonicalDeferredShaderBindings()
{
    using namespace Stoner::RHI;
    const auto Both = ERHIShaderStageFlags::Vertex | ERHIShaderStageFlags::Fragment;
    return {
        {0, 0, ERHIDescriptorType::UniformBuffer, Both},
        {1, 0, ERHIDescriptorType::UniformBuffer, Both},
        {2, 0, ERHIDescriptorType::CombinedTextureSampler, ERHIShaderStageFlags::Fragment},
        {2, 1, ERHIDescriptorType::CombinedTextureSampler, ERHIShaderStageFlags::Fragment},
        {2, 2, ERHIDescriptorType::CombinedTextureSampler, ERHIShaderStageFlags::Fragment},
        {2, 3, ERHIDescriptorType::CombinedTextureSampler, ERHIShaderStageFlags::Fragment},
        {2, 4, ERHIDescriptorType::CombinedTextureSampler, ERHIShaderStageFlags::Fragment},
        {3, 0, ERHIDescriptorType::StorageBuffer, Both},
    };
}

FDeferredVertexLayoutContract GetDeferredSurfaceVertexLayout()
{
    return {"Surface", 24, {{0, Stoner::RHI::ERHIFormat::R32G32B32_Float, 0},
        {1, Stoner::RHI::ERHIFormat::R32G32B32_Float, 12}}, true,
        Stoner::RHI::ERHIIndexType::UInt16};
}

FDeferredVertexLayoutContract GetDeferredFullscreenVertexLayout()
{
    return {"Fullscreen", 8, {{0, Stoner::RHI::ERHIFormat::R32G32_Float, 0}}, false,
        Stoner::RHI::ERHIIndexType::UInt16};
}

FDeferredVertexLayoutContract GetDeferredVolumeVertexLayout()
{
    return {"Volume", 12, {{0, Stoner::RHI::ERHIFormat::R32G32B32_Float, 0}}, true,
        Stoner::RHI::ERHIIndexType::UInt16};
}

FDeferredFrameViewUniform BuildDeferredFrameViewUniform(const FDeferredViewData& View)
{
    FDeferredFrameViewUniform Uniform;
    Uniform.View = PackRowMajorMatrixForShader(View.View);
    Uniform.Projection = PackRowMajorMatrixForShader(View.Projection);
    Uniform.InverseViewProjection = PackRowMajorMatrixForShader(View.InverseViewProjection);
    Uniform.ViewProjection = PackRowMajorMatrixForShader(View.ViewProjection);
    Uniform.CameraPosition = {View.CameraPosition.X, View.CameraPosition.Y, View.CameraPosition.Z, 1.0f};
    Uniform.OutputExtent = {static_cast<float>(View.Extent.Width), static_cast<float>(View.Extent.Height),
        1.0f / static_cast<float>(View.Extent.Width), 1.0f / static_cast<float>(View.Extent.Height)};
    Uniform.DepthConvention = {View.DepthPolicy.NearPlane, View.DepthPolicy.FarPlane,
        View.DepthPolicy.Convention == EDeferredDepthConvention::ReversedZ ? 1.0f : 0.0f, 0.0f};
    return Uniform;
}

FDeferredDrawMaterialUniform BuildDeferredDrawMaterialUniform(const FDeferredDrawRecord& Draw)
{
    FDeferredDrawMaterialUniform Uniform;
    Uniform.Model = PackRowMajorMatrixForShader(Draw.Candidate.Model);
    Uniform.WorldNormalFromModel = PackRowMajorMatrixForShader(Draw.WorldNormalFromModel);
    const auto& Surface = Draw.Candidate.Surface;
    Uniform.BaseColorAO = {Surface.BaseColor.R, Surface.BaseColor.G, Surface.BaseColor.B,
        Surface.AmbientOcclusion};
    Uniform.EmissiveMetallic = {Surface.Emissive.R, Surface.Emissive.G, Surface.Emissive.B,
        Surface.Metallic};
    Uniform.RoughnessAlphaCutoffFlags = {Surface.Roughness, Surface.Alpha, Surface.AlphaCutoff,
        Draw.Candidate.BlendMode == EMaterialBlendMode::Masked ? 1.0f : 0.0f};
    return Uniform;
}

FDeferredLightUniform BuildDeferredLightUniform(const FDeferredLightRecord& Light)
{
    FDeferredLightUniform Uniform;
    Uniform.PositionRange = {Light.Position.X, Light.Position.Y, Light.Position.Z, Light.Range};
    Uniform.DirectionOuterCos = {Light.Direction.X, Light.Direction.Y, Light.Direction.Z,
        std::cos(Light.OuterConeAngleRadians)};
    Uniform.ColorIntensity = {Light.Color.R, Light.Color.G, Light.Color.B, Light.Intensity};
    Uniform.InnerCosTypeVolumeMode = {std::cos(Light.InnerConeAngleRadians),
        static_cast<float>(Light.Type), static_cast<float>(Light.Acceptance), 0.0f};
    return Uniform;
}

} // namespace Stoner::Renderer
