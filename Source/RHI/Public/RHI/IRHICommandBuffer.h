#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIIndexType.h"
#include "RHI/FRHIIndexedDrawArguments.h"
#include "RHI/ERHIQueueType.h"
#include "RHI/ERHIResourceUsage.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIRenderPassDesc.h"
#include "RHI/FRHITextureBufferCopyRegion.h"

namespace Stoner::RHI
{

class IRHIBuffer;
class IRHIComputePipeline;
class IRHIDescriptorSet;
class IRHIFramebuffer;
class IRHIGraphicsPipeline;
class IRHIRenderPass;
class IRHITexture;

enum class ERHICommandBufferState
{
    Idle,
    Recording,
    Completed,
    Submitted,
    Resettable
};

enum class ERHISymbolicCommandType
{
    Draw,
    DrawIndexed,
    Dispatch,
    BindGraphicsPipeline,
    BindComputePipeline,
    Barrier,
    BufferCopy,
    TextureCopy,
    LayoutTransition,
    BeginRenderPass,
    EndRenderPass,
    UploadSchedule,
    BindVertexBuffer,
    BindIndexBuffer,
    BindDescriptorSet,
    TextureToBufferCopy,
    SetViewport,
    SetScissor
};

struct FRHIBufferCopyRange
{
    Stoner::Core::uint64 SourceOffsetBytes = 0;
    Stoner::Core::uint64 DestinationOffsetBytes = 0;
    Stoner::Core::uint64 SizeBytes = 0;
};

struct FRHITextureCopyRegion
{
    Stoner::Core::uint32 SourceMipLevel = 0;
    Stoner::Core::uint32 SourceArrayLayer = 0;
    Stoner::Core::uint32 DestinationMipLevel = 0;
    Stoner::Core::uint32 DestinationArrayLayer = 0;
    Stoner::Core::uint32 SourceX = 0;
    Stoner::Core::uint32 SourceY = 0;
    Stoner::Core::uint32 SourceZ = 0;
    Stoner::Core::uint32 DestinationX = 0;
    Stoner::Core::uint32 DestinationY = 0;
    Stoner::Core::uint32 DestinationZ = 0;
    Stoner::Core::uint32 Width = 1;
    Stoner::Core::uint32 Height = 1;
    Stoner::Core::uint32 Depth = 1;
};

enum class ERHIResourceLayout
{
    Undefined,
    General,
    CopySource,
    CopyDestination,
    ColorAttachment,
    DepthStencilAttachment,
    ShaderReadOnly,
    Present
};

struct FRHIResourceBarrierDesc
{
    Stoner::Core::TSharedPtr<IRHIBuffer> Buffer;
    Stoner::Core::TSharedPtr<IRHITexture> Texture;
    ERHIBufferUsage RequiredBufferUsage = ERHIBufferUsage::None;
    ERHITextureUsage RequiredTextureUsage = ERHITextureUsage::None;
    ERHIResourceLayout Before = ERHIResourceLayout::Undefined;
    ERHIResourceLayout After = ERHIResourceLayout::General;
};

struct FRHIViewport
{
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    float MinDepth = 0.0f;
    float MaxDepth = 1.0f;
};

struct FRHIScissorRect
{
    Stoner::Core::uint32 X = 0;
    Stoner::Core::uint32 Y = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
};

class IRHICommandBuffer
{
public:
    virtual ~IRHICommandBuffer() = default;

    [[nodiscard]] virtual ERHICommandBufferState GetState() const noexcept = 0;
    [[nodiscard]] virtual ERHIQueueType GetCompatibleQueueType() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetRecordedCommandCount() const noexcept = 0;

    virtual ERHIResult Begin() = 0;
    virtual ERHIResult End() = 0;
    virtual ERHIResult Reset() = 0;

    virtual ERHIResult RecordDraw(Stoner::Core::uint32 VertexCount, Stoner::Core::uint32 InstanceCount = 1) = 0;
    virtual ERHIResult RecordDrawIndexed(
        const FRHIIndexedDrawArguments& Arguments)
    {
        if (!IsValidRHIIndexedDrawArguments(Arguments))
        {
            return ERHIResult::InvalidState;
        }
        return RecordDrawIndexed(
            Arguments.IndexCount, Arguments.InstanceCount,
            Arguments.FirstInstance);
    }
    virtual ERHIResult RecordDrawIndexed(
        Stoner::Core::uint32 IndexCount,
        Stoner::Core::uint32 InstanceCount = 1,
        Stoner::Core::uint32 FirstInstance = 0)
    {
        (void)IndexCount;
        (void)InstanceCount;
        (void)FirstInstance;
        return ERHIResult::Unsupported;
    }
    virtual ERHIResult RecordDispatch(Stoner::Core::uint32 GroupCountX, Stoner::Core::uint32 GroupCountY, Stoner::Core::uint32 GroupCountZ) = 0;
    virtual ERHIResult BindGraphicsPipeline(const Stoner::Core::TSharedPtr<IRHIGraphicsPipeline>& Pipeline) = 0;
    virtual ERHIResult BindComputePipeline(const Stoner::Core::TSharedPtr<IRHIComputePipeline>& Pipeline) = 0;
    virtual ERHIResult RecordBarrier() = 0;
    virtual ERHIResult RecordBarrier(const FRHIResourceBarrierDesc& Barrier) = 0;
    virtual ERHIResult RecordBufferCopy(const Stoner::Core::TSharedPtr<IRHIBuffer>& Source, const Stoner::Core::TSharedPtr<IRHIBuffer>& Destination, FRHIBufferCopyRange Range) = 0;
    virtual ERHIResult RecordTextureCopy(const Stoner::Core::TSharedPtr<IRHITexture>& Source, const Stoner::Core::TSharedPtr<IRHITexture>& Destination, FRHITextureCopyRegion Region) = 0;
    virtual ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc& Transition) = 0;
    virtual ERHIResult BeginRenderPass(const Stoner::Core::TSharedPtr<IRHIRenderPass>& RenderPass, const Stoner::Core::TSharedPtr<IRHIFramebuffer>& Framebuffer) = 0;
    virtual ERHIResult BeginRenderPass(const Stoner::Core::TSharedPtr<IRHIRenderPass>&,
        const Stoner::Core::TSharedPtr<IRHIFramebuffer>&,
        const FRHIRenderPassClearValues&)
    {
        return ERHIResult::Unsupported;
    }
    virtual ERHIResult EndRenderPass() = 0;
    virtual ERHIResult BindVertexBuffer(const Stoner::Core::TSharedPtr<IRHIBuffer>&, Stoner::Core::uint64 = 0)
    {
        return ERHIResult::Unsupported;
    }
    virtual ERHIResult BindIndexBuffer(const Stoner::Core::TSharedPtr<IRHIBuffer>&,
        ERHIIndexType, Stoner::Core::uint64 = 0)
    {
        return ERHIResult::Unsupported;
    }
    virtual ERHIResult BindDescriptorSet(const Stoner::Core::TSharedPtr<IRHIDescriptorSet>&)
    {
        return ERHIResult::Unsupported;
    }
    virtual ERHIResult RecordTextureToBufferCopy(const Stoner::Core::TSharedPtr<IRHITexture>&,
        const Stoner::Core::TSharedPtr<IRHIBuffer>&, FRHITextureBufferCopyRegion)
    {
        return ERHIResult::Unsupported;
    }
    virtual ERHIResult SetViewport(const FRHIViewport&) { return ERHIResult::Unsupported; }
    virtual ERHIResult SetScissor(const FRHIScissorRect&) { return ERHIResult::Unsupported; }
};

} // namespace Stoner::RHI
