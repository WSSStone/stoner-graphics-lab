#pragma once

#include "Renderer/FForwardFramePlan.h"
#include "RHI/ERHIIndexType.h"
#include "RHI/FRHIIndexedDrawArguments.h"
#include "RHI/FRHITextureBufferCopyRegion.h"

namespace Stoner::RHI
{
class IRHIBuffer;
class IRHICommandBuffer;
class IRHIFramebuffer;
class IRHIGraphicsPipeline;
class IRHIRenderPass;
class IRHITexture;
class IRHIDescriptorSet;
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
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<
        Stoner::RHI::IRHITexture>> AuxiliaryColorTextures;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> DepthTexture;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> VertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline> GraphicsPipeline;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass> RenderPass;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer> Framebuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer> CommandBuffer;
    struct FDrawBinding
    {
        Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> VertexBuffer;
        Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> IndexBuffer;
        Stoner::RHI::ERHIIndexType IndexType =
            Stoner::RHI::ERHIIndexType::UInt16;
        Stoner::RHI::FRHIIndexedDrawArguments Draw;
        Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline> Pipeline;
        Stoner::Core::TArray<Stoner::Core::TSharedPtr<
            Stoner::RHI::IRHIDescriptorSet>> DescriptorSets;
    };
    Stoner::Core::TArray<FDrawBinding> Draws;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> ReadbackBuffer;
    Stoner::RHI::FRHITextureBufferCopyRegion ReadbackRegion;
    bool bTransitionToPresent = true;
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
