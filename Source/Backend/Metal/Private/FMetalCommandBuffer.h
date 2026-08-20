#pragma once

#include "FMetalDescriptorSet.h"
#include "FMetalNativeObject.h"
#include "RHI/IRHICommandBuffer.h"

#include <mutex>

namespace Stoner::Backend::Metal::Private
{

struct FMetalCommandRecord
{
    RHI::ERHISymbolicCommandType Type = RHI::ERHISymbolicCommandType::Barrier;
    Core::TSharedPtr<RHI::IRHIBuffer> BufferA;
    Core::TSharedPtr<RHI::IRHIBuffer> BufferB;
    Core::TSharedPtr<RHI::IRHITexture> TextureA;
    Core::TSharedPtr<RHI::IRHITexture> TextureB;
    Core::TSharedPtr<RHI::IRHIGraphicsPipeline> GraphicsPipeline;
    Core::TSharedPtr<RHI::IRHIComputePipeline> ComputePipeline;
    Core::TSharedPtr<RHI::IRHIDescriptorSet> DescriptorSet;
    FMetalDescriptorSnapshot DescriptorSnapshot;
    Core::uint32 DescriptorSetIndex = 0;
    Core::TSharedPtr<RHI::IRHIRenderPass> RenderPass;
    Core::TSharedPtr<RHI::IRHIFramebuffer> Framebuffer;
    RHI::FRHIRenderPassClearValues ClearValues;
    RHI::FRHIResourceBarrierDesc Barrier;
    RHI::FRHIBufferCopyRange BufferCopy;
    RHI::FRHITextureCopyRegion TextureCopy;
    RHI::FRHITextureBufferCopyRegion TextureToBufferCopy;
    RHI::FRHIIndexedDrawArguments IndexedDraw;
    RHI::FRHIViewport Viewport;
    RHI::FRHIScissorRect Scissor;
    RHI::ERHIIndexType IndexType = RHI::ERHIIndexType::UInt16;
    Core::uint64 A = 0;
    Core::uint64 B = 0;
    Core::uint64 C = 0;
};

class FMetalCommandBuffer final
    : public RHI::IRHICommandBuffer,
      public FMetalNativeObject
{
public:
    FMetalCommandBuffer(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::ERHIQueueType QueueType) noexcept;
    ~FMetalCommandBuffer() override;

    [[nodiscard]] RHI::ERHICommandBufferState GetState()
        const noexcept override;
    [[nodiscard]] RHI::ERHIQueueType GetCompatibleQueueType()
        const noexcept override;
    [[nodiscard]] Core::uint32 GetRecordedCommandCount() const noexcept override;
    RHI::ERHIResult Begin() override;
    RHI::ERHIResult End() override;
    RHI::ERHIResult Reset() override;
    RHI::ERHIResult RecordDraw(
        Core::uint32 VertexCount, Core::uint32 InstanceCount = 1) override;
    RHI::ERHIResult RecordDrawIndexed(
        const RHI::FRHIIndexedDrawArguments& Arguments) override;
    RHI::ERHIResult RecordDrawIndexed(
        Core::uint32 IndexCount, Core::uint32 InstanceCount = 1,
        Core::uint32 FirstInstance = 0) override;
    RHI::ERHIResult RecordDispatch(
        Core::uint32 GroupCountX, Core::uint32 GroupCountY,
        Core::uint32 GroupCountZ) override;
    RHI::ERHIResult BindGraphicsPipeline(
        const Core::TSharedPtr<RHI::IRHIGraphicsPipeline>& Pipeline) override;
    RHI::ERHIResult BindComputePipeline(
        const Core::TSharedPtr<RHI::IRHIComputePipeline>& Pipeline) override;
    RHI::ERHIResult RecordBarrier() override;
    RHI::ERHIResult RecordBarrier(
        const RHI::FRHIResourceBarrierDesc& Barrier) override;
    RHI::ERHIResult RecordBufferCopy(
        const Core::TSharedPtr<RHI::IRHIBuffer>& Source,
        const Core::TSharedPtr<RHI::IRHIBuffer>& Destination,
        RHI::FRHIBufferCopyRange Range) override;
    RHI::ERHIResult RecordTextureCopy(
        const Core::TSharedPtr<RHI::IRHITexture>& Source,
        const Core::TSharedPtr<RHI::IRHITexture>& Destination,
        RHI::FRHITextureCopyRegion Region) override;
    RHI::ERHIResult RecordLayoutTransition(
        const RHI::FRHIResourceBarrierDesc& Transition) override;
    RHI::ERHIResult BeginRenderPass(
        const Core::TSharedPtr<RHI::IRHIRenderPass>& RenderPass,
        const Core::TSharedPtr<RHI::IRHIFramebuffer>& Framebuffer) override;
    RHI::ERHIResult BeginRenderPass(
        const Core::TSharedPtr<RHI::IRHIRenderPass>& RenderPass,
        const Core::TSharedPtr<RHI::IRHIFramebuffer>& Framebuffer,
        const RHI::FRHIRenderPassClearValues& ClearValues) override;
    RHI::ERHIResult EndRenderPass() override;
    RHI::ERHIResult BindVertexBuffer(
        const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
        Core::uint64 OffsetBytes = 0) override;
    RHI::ERHIResult BindIndexBuffer(
        const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
        RHI::ERHIIndexType IndexType,
        Core::uint64 OffsetBytes = 0) override;
    RHI::ERHIResult BindDescriptorSet(
        const Core::TSharedPtr<RHI::IRHIDescriptorSet>& DescriptorSet) override;
    RHI::ERHIResult RecordTextureToBufferCopy(
        const Core::TSharedPtr<RHI::IRHITexture>& Source,
        const Core::TSharedPtr<RHI::IRHIBuffer>& Destination,
        RHI::FRHITextureBufferCopyRegion Region) override;
    RHI::ERHIResult SetViewport(const RHI::FRHIViewport& Viewport) override;
    RHI::ERHIResult SetScissor(const RHI::FRHIScissorRect& Scissor) override;

    [[nodiscard]] bool IsCompatibleWith(
        const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept;
    [[nodiscard]] bool PrepareSubmission(
        Core::TArray<FMetalCommandRecord>& OutRecords) noexcept;
    void CompleteSubmission() noexcept;

private:
    [[nodiscard]] bool IsRecording() const noexcept;
    [[nodiscard]] bool SupportsTransfer() const noexcept;
    [[nodiscard]] bool SupportsCompute() const noexcept;
    [[nodiscard]] RHI::ERHIResult Append(FMetalCommandRecord Record) noexcept;
    void ClearRecordingState() noexcept;

    mutable std::mutex Mutex_;
    RHI::ERHIQueueType QueueType_ = RHI::ERHIQueueType::Graphics;
    RHI::ERHICommandBufferState State_ = RHI::ERHICommandBufferState::Idle;
    Core::TArray<FMetalCommandRecord> Records_;
    Core::TSharedPtr<RHI::IRHIRenderPass> ActiveRenderPass_;
    Core::TSharedPtr<RHI::IRHIFramebuffer> ActiveFramebuffer_;
    Core::TSharedPtr<RHI::IRHIGraphicsPipeline> BoundGraphicsPipeline_;
    Core::TSharedPtr<RHI::IRHIComputePipeline> BoundComputePipeline_;
    Core::TSharedPtr<RHI::IRHIBuffer> BoundVertexBuffer_;
    Core::TSharedPtr<RHI::IRHIBuffer> BoundIndexBuffer_;
};

} // namespace Stoner::Backend::Metal::Private
