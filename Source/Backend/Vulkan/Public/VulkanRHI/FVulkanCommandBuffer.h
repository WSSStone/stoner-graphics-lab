#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanFramebuffer;
class FVulkanComputePipeline;
class FVulkanCommandPool;
class FVulkanCommandSubmission;
class FVulkanDevice;
class FVulkanGraphicsPipeline;
class FVulkanQueue;
class FVulkanRenderPass;
class FVulkanUploadRequest;
struct FVulkanDeviceOwnerState;
struct FVulkanDiagnostics;

struct FVulkanRecordedCommand
{
    Stoner::RHI::ERHISymbolicCommandType Type = Stoner::RHI::ERHISymbolicCommandType::Draw;
    Stoner::Core::uint64 A = 0;
    Stoner::Core::uint64 B = 0;
    Stoner::Core::uint64 C = 0;
    Stoner::Core::int64 D = 0;
    Stoner::Core::uint64 E = 0;
};

class FVulkanCommandBuffer final : public Stoner::RHI::IRHICommandBuffer
{
public:
    ~FVulkanCommandBuffer() override = default;
    FVulkanCommandBuffer(const FVulkanCommandBuffer&) = delete;
    FVulkanCommandBuffer& operator=(const FVulkanCommandBuffer&) = delete;

    [[nodiscard]] Stoner::RHI::ERHICommandBufferState GetState() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIQueueType GetCompatibleQueueType() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetRecordedCommandCount() const noexcept override;
    [[nodiscard]] const Stoner::Core::TArray<FVulkanRecordedCommand>& GetRecordedCommands() const noexcept;
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool HasActiveRenderPass() const noexcept;

    Stoner::RHI::ERHIResult Begin() override;
    Stoner::RHI::ERHIResult End() override;
    Stoner::RHI::ERHIResult Reset() override;

    Stoner::RHI::ERHIResult RecordDraw(Stoner::Core::uint32 VertexCount, Stoner::Core::uint32 InstanceCount = 1) override;
    Stoner::RHI::ERHIResult RecordDrawIndexed(
        const Stoner::RHI::FRHIIndexedDrawArguments& Arguments) override;
    Stoner::RHI::ERHIResult RecordDrawIndexed(
        Stoner::Core::uint32 IndexCount,
        Stoner::Core::uint32 InstanceCount = 1,
        Stoner::Core::uint32 FirstInstance = 0) override;
    Stoner::RHI::ERHIResult RecordDispatch(Stoner::Core::uint32 GroupCountX, Stoner::Core::uint32 GroupCountY, Stoner::Core::uint32 GroupCountZ) override;
    Stoner::RHI::ERHIResult BindGraphicsPipeline(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline>& Pipeline) override;
    Stoner::RHI::ERHIResult BindComputePipeline(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIComputePipeline>& Pipeline) override;
    Stoner::RHI::ERHIResult RecordBarrier() override;
    Stoner::RHI::ERHIResult RecordBarrier(const Stoner::RHI::FRHIResourceBarrierDesc& Barrier) override;
    Stoner::RHI::ERHIResult RecordBufferCopy(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Source, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Destination, Stoner::RHI::FRHIBufferCopyRange Range) override;
    Stoner::RHI::ERHIResult RecordTextureCopy(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Source, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Destination, Stoner::RHI::FRHITextureCopyRegion Region) override;
    Stoner::RHI::ERHIResult RecordLayoutTransition(const Stoner::RHI::FRHIResourceBarrierDesc& Transition) override;
    Stoner::RHI::ERHIResult BeginRenderPass(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass>& RenderPass, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer>& Framebuffer) override;
    Stoner::RHI::ERHIResult BeginRenderPass(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass>& RenderPass,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer>& Framebuffer,
        const Stoner::RHI::FRHIRenderPassClearValues& ClearValues) override;
    Stoner::RHI::ERHIResult EndRenderPass() override;
    Stoner::RHI::ERHIResult BindVertexBuffer(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer, Stoner::Core::uint64 OffsetBytes = 0) override;
    Stoner::RHI::ERHIResult BindIndexBuffer(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer,
        Stoner::RHI::ERHIIndexType IndexType, Stoner::Core::uint64 OffsetBytes = 0) override;
    Stoner::RHI::ERHIResult BindDescriptorSet(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDescriptorSet>& DescriptorSet) override;
    Stoner::RHI::ERHIResult RecordTextureToBufferCopy(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Source,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Destination,
        Stoner::RHI::FRHITextureBufferCopyRegion Region) override;
    Stoner::RHI::ERHIResult SetViewport(const Stoner::RHI::FRHIViewport& Viewport) override;
    Stoner::RHI::ERHIResult SetScissor(const Stoner::RHI::FRHIScissorRect& Scissor) override;
    Stoner::RHI::ERHIResult ScheduleBufferUpload(const Stoner::Core::TSharedPtr<FVulkanUploadRequest>& Upload);
    Stoner::RHI::ERHIResult ScheduleTextureUpload(const Stoner::Core::TSharedPtr<FVulkanUploadRequest>& Upload);

private:
    friend class FVulkanCommandPool;
    friend class FVulkanCommandSubmission;
    friend class FVulkanDevice;
    friend class FVulkanQueue;

    FVulkanCommandBuffer(Stoner::RHI::ERHIQueueType InQueueType,
        FVulkanDiagnostics* InDiagnostics,
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept;
    [[nodiscard]] bool BelongsTo(
        const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept;
    Stoner::RHI::ERHIResult MarkSubmitted() noexcept;
    Stoner::RHI::ERHIResult MarkCompletedOrResettable() noexcept;
    void Invalidate() noexcept;

    [[nodiscard]] bool IsTransferCompatible() const noexcept;
    [[nodiscard]] bool IsComputeCompatible() const noexcept;
    [[nodiscard]] bool HasCompatibleGraphicsPipeline() const noexcept;
    [[nodiscard]] bool HasCompatibleComputePipeline() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ValidateRecordingState() const noexcept;
    void AppendCommand(FVulkanRecordedCommand Command);
    void MarkRecordingDiagnostic(const char* Reason) noexcept;

    Stoner::RHI::ERHIQueueType QueueType = Stoner::RHI::ERHIQueueType::Graphics;
    Stoner::RHI::ERHICommandBufferState State = Stoner::RHI::ERHICommandBufferState::Idle;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    Stoner::Core::TArray<FVulkanRecordedCommand> Commands;
    FVulkanDiagnostics* Diagnostics = nullptr;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIRenderPass> ActiveRenderPass;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIFramebuffer> ActiveFramebuffer;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIGraphicsPipeline> BoundGraphicsPipeline;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIComputePipeline> BoundComputePipeline;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIBuffer> BoundVertexBuffer;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIBuffer> BoundIndexBuffer;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
