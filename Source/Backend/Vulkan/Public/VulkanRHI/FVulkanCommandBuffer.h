#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanFramebuffer;
class FVulkanRenderPass;
class FVulkanUploadRequest;
struct FVulkanDiagnostics;

struct FVulkanRecordedCommand
{
    Stoner::RHI::ERHISymbolicCommandType Type = Stoner::RHI::ERHISymbolicCommandType::Draw;
    Stoner::Core::uint64 A = 0;
    Stoner::Core::uint64 B = 0;
    Stoner::Core::uint64 C = 0;
};

class FVulkanCommandBuffer final : public Stoner::RHI::IRHICommandBuffer
{
public:
    FVulkanCommandBuffer(Stoner::RHI::ERHIQueueType InQueueType, FVulkanDiagnostics* InDiagnostics) noexcept;

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
    Stoner::RHI::ERHIResult RecordDrawIndexed(Stoner::Core::uint32 IndexCount, Stoner::Core::uint32 InstanceCount = 1) override;
    Stoner::RHI::ERHIResult RecordDispatch(Stoner::Core::uint32 GroupCountX, Stoner::Core::uint32 GroupCountY, Stoner::Core::uint32 GroupCountZ) override;
    Stoner::RHI::ERHIResult RecordBarrier() override;
    Stoner::RHI::ERHIResult RecordBarrier(const Stoner::RHI::FRHIResourceBarrierDesc& Barrier) override;
    Stoner::RHI::ERHIResult RecordBufferCopy(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Source, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Destination, Stoner::RHI::FRHIBufferCopyRange Range) override;
    Stoner::RHI::ERHIResult RecordTextureCopy(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Source, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Destination, Stoner::RHI::FRHITextureCopyRegion Region) override;
    Stoner::RHI::ERHIResult RecordLayoutTransition(const Stoner::RHI::FRHIResourceBarrierDesc& Transition) override;
    Stoner::RHI::ERHIResult BeginRenderPass(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass>& RenderPass, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer>& Framebuffer) override;
    Stoner::RHI::ERHIResult EndRenderPass() override;
    Stoner::RHI::ERHIResult ScheduleBufferUpload(const Stoner::Core::TSharedPtr<FVulkanUploadRequest>& Upload);
    Stoner::RHI::ERHIResult ScheduleTextureUpload(const Stoner::Core::TSharedPtr<FVulkanUploadRequest>& Upload);

    Stoner::RHI::ERHIResult MarkSubmitted() noexcept;
    Stoner::RHI::ERHIResult MarkCompletedOrResettable() noexcept;
    void Invalidate() noexcept;

private:
    [[nodiscard]] bool IsTransferCompatible() const noexcept;
    [[nodiscard]] bool IsComputeCompatible() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ValidateRecordingState() const noexcept;
    void AppendCommand(FVulkanRecordedCommand Command);
    void MarkRecordingDiagnostic(const char* Reason) noexcept;

    Stoner::RHI::ERHIQueueType QueueType = Stoner::RHI::ERHIQueueType::Graphics;
    Stoner::RHI::ERHICommandBufferState State = Stoner::RHI::ERHICommandBufferState::Idle;
    Stoner::Core::TArray<FVulkanRecordedCommand> Commands;
    FVulkanDiagnostics* Diagnostics = nullptr;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIRenderPass> ActiveRenderPass;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIFramebuffer> ActiveFramebuffer;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
