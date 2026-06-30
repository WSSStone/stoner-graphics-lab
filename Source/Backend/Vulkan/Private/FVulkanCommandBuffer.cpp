#include "VulkanRHI/FVulkanCommandBuffer.h"

#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanFramebuffer.h"
#include "VulkanRHI/FVulkanRenderPass.h"
#include "VulkanRHI/FVulkanUploadStaging.h"

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] bool IsValidResource(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer) noexcept
{
    return Buffer && Buffer->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid;
}

[[nodiscard]] bool IsValidResource(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture) noexcept
{
    return Texture && Texture->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid;
}

[[nodiscard]] bool BufferRangeFits(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer, Stoner::Core::uint64 OffsetBytes, Stoner::Core::uint64 SizeBytes) noexcept
{
    return IsValidResource(Buffer) && SizeBytes > 0 && OffsetBytes <= Buffer->GetSizeInBytes() && SizeBytes <= Buffer->GetSizeInBytes() - OffsetBytes;
}

[[nodiscard]] bool TextureRegionFits(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture, Stoner::Core::uint32 MipLevel, Stoner::Core::uint32 ArrayLayer, Stoner::Core::uint32 X, Stoner::Core::uint32 Y, Stoner::Core::uint32 Z, Stoner::Core::uint32 Width, Stoner::Core::uint32 Height, Stoner::Core::uint32 Depth) noexcept
{
    if (!IsValidResource(Texture) || Width == 0 || Height == 0 || Depth == 0)
    {
        return false;
    }
    const Stoner::RHI::FRHITextureDesc& Desc = Texture->GetDesc();
    return MipLevel < Desc.MipLevels && ArrayLayer < Desc.ArrayLayers &&
        X <= Desc.Width && Width <= Desc.Width - X &&
        Y <= Desc.Height && Height <= Desc.Height - Y &&
        Z <= Desc.Depth && Depth <= Desc.Depth - Z;
}

} // namespace

FVulkanCommandBuffer::FVulkanCommandBuffer(Stoner::RHI::ERHIQueueType InQueueType, FVulkanDiagnostics* InDiagnostics) noexcept
    : QueueType(InQueueType)
    , Diagnostics(InDiagnostics)
{
}

Stoner::RHI::ERHICommandBufferState FVulkanCommandBuffer::GetState() const noexcept { return State; }
Stoner::RHI::ERHIQueueType FVulkanCommandBuffer::GetCompatibleQueueType() const noexcept { return QueueType; }
Stoner::Core::uint32 FVulkanCommandBuffer::GetRecordedCommandCount() const noexcept { return static_cast<Stoner::Core::uint32>(Commands.size()); }
const Stoner::Core::TArray<FVulkanRecordedCommand>& FVulkanCommandBuffer::GetRecordedCommands() const noexcept { return Commands; }
bool FVulkanCommandBuffer::IsValid() const noexcept { return bValid; }
bool FVulkanCommandBuffer::HasActiveRenderPass() const noexcept { return !ActiveRenderPass.expired(); }

Stoner::RHI::ERHIResult FVulkanCommandBuffer::Begin()
{
    if (!bValid || (State != Stoner::RHI::ERHICommandBufferState::Idle && State != Stoner::RHI::ERHICommandBufferState::Resettable))
    {
        MarkRecordingDiagnostic("command buffer begin rejected by lifecycle state");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    Commands.clear();
    ActiveRenderPass.reset();
    ActiveFramebuffer.reset();
    State = Stoner::RHI::ERHICommandBufferState::Recording;
    MarkRecordingDiagnostic("command buffer recording began");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::End()
{
    if (!bValid || State != Stoner::RHI::ERHICommandBufferState::Recording || HasActiveRenderPass())
    {
        MarkRecordingDiagnostic("command buffer end rejected by lifecycle state or open render pass");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    State = Stoner::RHI::ERHICommandBufferState::Completed;
    MarkRecordingDiagnostic("command buffer recording completed");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::Reset()
{
    if (!bValid || State == Stoner::RHI::ERHICommandBufferState::Recording || State == Stoner::RHI::ERHICommandBufferState::Submitted)
    {
        MarkRecordingDiagnostic("command buffer reset rejected by lifecycle state");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    Commands.clear();
    ActiveRenderPass.reset();
    ActiveFramebuffer.reset();
    State = Stoner::RHI::ERHICommandBufferState::Idle;
    MarkRecordingDiagnostic("command buffer reset");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordDraw(Stoner::Core::uint32 VertexCount, Stoner::Core::uint32 InstanceCount)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics || !HasActiveRenderPass() || VertexCount == 0 || InstanceCount == 0)
    {
        MarkRecordingDiagnostic("draw rejected; requires graphics recording inside render pass");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::Draw, VertexCount, InstanceCount, 0});
    MarkRecordingDiagnostic("draw placeholder recorded without bound pipeline");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordDrawIndexed(Stoner::Core::uint32 IndexCount, Stoner::Core::uint32 InstanceCount)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics || !HasActiveRenderPass() || IndexCount == 0 || InstanceCount == 0)
    {
        MarkRecordingDiagnostic("indexed draw rejected; requires graphics recording inside render pass");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::DrawIndexed, IndexCount, InstanceCount, 0});
    MarkRecordingDiagnostic("indexed draw placeholder recorded without bound pipeline");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordDispatch(Stoner::Core::uint32 GroupCountX, Stoner::Core::uint32 GroupCountY, Stoner::Core::uint32 GroupCountZ)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsComputeCompatible() || GroupCountX == 0 || GroupCountY == 0 || GroupCountZ == 0)
    {
        MarkRecordingDiagnostic("dispatch rejected; requires compute-compatible recording state");
        return QueueType == Stoner::RHI::ERHIQueueType::Transfer ? Stoner::RHI::ERHIResult::Unsupported : Stoner::RHI::ERHIResult::InvalidState;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::Dispatch, GroupCountX, GroupCountY, GroupCountZ});
    MarkRecordingDiagnostic("dispatch placeholder recorded without bound pipeline");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordBarrier()
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success)
    {
        MarkRecordingDiagnostic("generic barrier rejected outside recording");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::Barrier, 0, 0, 0});
    MarkRecordingDiagnostic("generic barrier recorded");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordBarrier(const Stoner::RHI::FRHIResourceBarrierDesc& Barrier)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || Barrier.Before == Barrier.After)
    {
        MarkRecordingDiagnostic("barrier rejected by state consistency");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Barrier.Buffer)
    {
        if (!IsValidResource(Barrier.Buffer) || (Barrier.RequiredBufferUsage != Stoner::RHI::ERHIBufferUsage::None && !Stoner::RHI::HasRHIFlag(Barrier.Buffer->GetUsage(), Barrier.RequiredBufferUsage)))
        {
            MarkRecordingDiagnostic("buffer barrier rejected by lifecycle or usage");
            return Stoner::RHI::ERHIResult::Unsupported;
        }
    }
    else if (Barrier.Texture)
    {
        if (!IsValidResource(Barrier.Texture) || (Barrier.RequiredTextureUsage != Stoner::RHI::ERHITextureUsage::None && !Stoner::RHI::HasRHIFlag(Barrier.Texture->GetUsage(), Barrier.RequiredTextureUsage)))
        {
            MarkRecordingDiagnostic("texture barrier rejected by lifecycle or usage");
            return Stoner::RHI::ERHIResult::Unsupported;
        }
    }
    else
    {
        MarkRecordingDiagnostic("barrier rejected because no resource was provided");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::Barrier, static_cast<Stoner::Core::uint64>(Barrier.Before), static_cast<Stoner::Core::uint64>(Barrier.After), 0});
    MarkRecordingDiagnostic("declarative barrier recorded");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordBufferCopy(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Source, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Destination, Stoner::RHI::FRHIBufferCopyRange Range)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsTransferCompatible())
    {
        MarkRecordingDiagnostic("buffer copy rejected by queue capability");
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    if (!BufferRangeFits(Source, Range.SourceOffsetBytes, Range.SizeBytes) || !BufferRangeFits(Destination, Range.DestinationOffsetBytes, Range.SizeBytes) ||
        !Stoner::RHI::HasRHIFlag(Source->GetUsage(), Stoner::RHI::ERHIBufferUsage::CopySource) ||
        !Stoner::RHI::HasRHIFlag(Destination->GetUsage(), Stoner::RHI::ERHIBufferUsage::CopyDestination))
    {
        MarkRecordingDiagnostic("buffer copy rejected by resource lifecycle range or usage");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::BufferCopy, Range.SourceOffsetBytes, Range.DestinationOffsetBytes, Range.SizeBytes});
    MarkRecordingDiagnostic("buffer copy recorded");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordTextureCopy(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Source, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Destination, Stoner::RHI::FRHITextureCopyRegion Region)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsTransferCompatible())
    {
        MarkRecordingDiagnostic("texture copy rejected by queue capability");
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    if (!TextureRegionFits(Source, Region.SourceMipLevel, Region.SourceArrayLayer, Region.SourceX, Region.SourceY, Region.SourceZ, Region.Width, Region.Height, Region.Depth) ||
        !TextureRegionFits(Destination, Region.DestinationMipLevel, Region.DestinationArrayLayer, Region.DestinationX, Region.DestinationY, Region.DestinationZ, Region.Width, Region.Height, Region.Depth) ||
        !Stoner::RHI::HasRHIFlag(Source->GetUsage(), Stoner::RHI::ERHITextureUsage::CopySource) ||
        !Stoner::RHI::HasRHIFlag(Destination->GetUsage(), Stoner::RHI::ERHITextureUsage::CopyDestination))
    {
        MarkRecordingDiagnostic("texture copy rejected by resource lifecycle region or usage");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::TextureCopy, Region.Width, Region.Height, Region.Depth});
    MarkRecordingDiagnostic("texture copy recorded");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordLayoutTransition(const Stoner::RHI::FRHIResourceBarrierDesc& Transition)
{
    const Stoner::RHI::ERHIResult Result = RecordBarrier(Transition);
    if (Result == Stoner::RHI::ERHIResult::Success && !Commands.empty())
    {
        Commands.back().Type = Stoner::RHI::ERHISymbolicCommandType::LayoutTransition;
        MarkRecordingDiagnostic("declarative layout transition recorded");
    }
    return Result;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::BeginRenderPass(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass>& RenderPass, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer>& Framebuffer)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics || HasActiveRenderPass() ||
        !RenderPass || !Framebuffer || RenderPass->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Framebuffer->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid || Framebuffer->GetRenderPass() != RenderPass)
    {
        MarkRecordingDiagnostic("begin render pass rejected by scope lifecycle or compatibility");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    ActiveRenderPass = RenderPass;
    ActiveFramebuffer = Framebuffer;
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::BeginRenderPass, Framebuffer->GetWidth(), Framebuffer->GetHeight(), Framebuffer->GetAttachmentCount()});
    MarkRecordingDiagnostic("render pass scope began");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::EndRenderPass()
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !HasActiveRenderPass())
    {
        MarkRecordingDiagnostic("end render pass rejected without active scope");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    ActiveRenderPass.reset();
    ActiveFramebuffer.reset();
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::EndRenderPass, 0, 0, 0});
    MarkRecordingDiagnostic("render pass scope ended");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::ScheduleBufferUpload(const Stoner::Core::TSharedPtr<FVulkanUploadRequest>& Upload)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsTransferCompatible() || !Upload || Upload->GetKind() != EVulkanUploadKind::Buffer || Upload->GetLifecycle() != EVulkanUploadLifecycle::Pending)
    {
        MarkRecordingDiagnostic("buffer upload scheduling rejected");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto Buffer = Upload->GetBuffer();
    const FVulkanBufferUploadRange Range = Upload->GetBufferRange();
    if (!BufferRangeFits(Buffer, Range.OffsetBytes, Range.SizeBytes))
    {
        MarkRecordingDiagnostic("buffer upload scheduling rejected by destination range");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const Stoner::RHI::ERHIResult Result = Upload->MarkScheduled();
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::UploadSchedule, Range.OffsetBytes, Range.SizeBytes, 0});
    if (Diagnostics)
    {
        MarkUploadScheduling(*Diagnostics, "buffer upload scheduled into command buffer");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::ScheduleTextureUpload(const Stoner::Core::TSharedPtr<FVulkanUploadRequest>& Upload)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsTransferCompatible() || !Upload || Upload->GetKind() != EVulkanUploadKind::Texture || Upload->GetLifecycle() != EVulkanUploadLifecycle::Pending)
    {
        MarkRecordingDiagnostic("texture upload scheduling rejected");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto Texture = Upload->GetTexture();
    const FVulkanTextureUploadRegion Region = Upload->GetTextureRegion();
    if (!TextureRegionFits(Texture, Region.MipLevel, Region.ArrayLayer, Region.X, Region.Y, Region.Z, Region.Width, Region.Height, Region.Depth))
    {
        MarkRecordingDiagnostic("texture upload scheduling rejected by destination region");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const Stoner::RHI::ERHIResult Result = Upload->MarkScheduled();
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }
    AppendCommand({Stoner::RHI::ERHISymbolicCommandType::UploadSchedule, Region.Width, Region.Height, Region.Depth});
    if (Diagnostics)
    {
        MarkUploadScheduling(*Diagnostics, "texture upload scheduled into command buffer");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::MarkSubmitted() noexcept
{
    if (!bValid || State != Stoner::RHI::ERHICommandBufferState::Completed || Commands.empty())
    {
        MarkRecordingDiagnostic("command buffer submission state transition rejected");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    State = Stoner::RHI::ERHICommandBufferState::Submitted;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::MarkCompletedOrResettable() noexcept
{
    if (!bValid || State != Stoner::RHI::ERHICommandBufferState::Submitted)
    {
        MarkRecordingDiagnostic("command buffer completion transition rejected");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    State = Stoner::RHI::ERHICommandBufferState::Resettable;
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanCommandBuffer::Invalidate() noexcept
{
    bValid = false;
    Commands.clear();
    ActiveRenderPass.reset();
    ActiveFramebuffer.reset();
}

bool FVulkanCommandBuffer::IsTransferCompatible() const noexcept
{
    return QueueType == Stoner::RHI::ERHIQueueType::Graphics || QueueType == Stoner::RHI::ERHIQueueType::Compute || QueueType == Stoner::RHI::ERHIQueueType::Transfer;
}

bool FVulkanCommandBuffer::IsComputeCompatible() const noexcept
{
    return QueueType == Stoner::RHI::ERHIQueueType::Graphics || QueueType == Stoner::RHI::ERHIQueueType::Compute;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::ValidateRecordingState() const noexcept
{
    return bValid && State == Stoner::RHI::ERHICommandBufferState::Recording ? Stoner::RHI::ERHIResult::Success : Stoner::RHI::ERHIResult::InvalidState;
}

void FVulkanCommandBuffer::AppendCommand(FVulkanRecordedCommand Command)
{
    Commands.push_back(Command);
}

void FVulkanCommandBuffer::MarkRecordingDiagnostic(const char* Reason) noexcept
{
    if (Diagnostics)
    {
        MarkCommandRecording(*Diagnostics, Reason);
    }
}

} // namespace Stoner::Backend::Vulkan
