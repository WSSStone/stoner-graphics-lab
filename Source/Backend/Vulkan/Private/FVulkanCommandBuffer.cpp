#include "VulkanRHI/FVulkanCommandBuffer.h"

#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "RHI/IRHIDescriptorSet.h"
#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanFramebuffer.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"
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
    return IsValidResource(Texture) &&
        Stoner::RHI::IsRHITextureRegionValid(Texture->GetDesc(),
            MipLevel, ArrayLayer, X, Y, Z, Width, Height, Depth);
}

[[nodiscard]] bool TexturesAreCopyCompatible(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Source,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Destination) noexcept
{
    return Source && Destination &&
        Source->GetDimension() == Destination->GetDimension() &&
        Source->GetFormat() == Destination->GetFormat() &&
        Source->GetDesc().SampleCount == Stoner::RHI::ERHISampleCount::One &&
        Destination->GetDesc().SampleCount == Stoner::RHI::ERHISampleCount::One;
}

[[nodiscard]] bool ClearValuesMatch(const Stoner::RHI::IRHIRenderPass& RenderPass,
    const Stoner::RHI::FRHIRenderPassClearValues& ClearValues) noexcept
{
    Stoner::Core::uint32 ColorClearCount = 0;
    bool bNeedsDepthClear = false;
    for (const Stoner::RHI::FRHIRenderPassAttachmentDesc& Attachment : RenderPass.GetDesc().Attachments)
    {
        if (Attachment.LoadOp != Stoner::RHI::ERHIAttachmentLoadOp::Clear)
        {
            continue;
        }
        if (Attachment.Role == Stoner::RHI::ERHIAttachmentRole::Color)
        {
            ++ColorClearCount;
        }
        else
        {
            bNeedsDepthClear = true;
        }
    }
    return ClearValues.Colors.size() == ColorClearCount &&
        (!bNeedsDepthClear || (ClearValues.Depth >= 0.0f && ClearValues.Depth <= 1.0f));
}

} // namespace

FVulkanCommandBuffer::FVulkanCommandBuffer(
    Stoner::RHI::ERHIQueueType InQueueType,
    FVulkanDiagnostics* InDiagnostics,
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept
    : QueueType(InQueueType)
    , Owner(std::move(InOwner))
    , Diagnostics(InDiagnostics)
{
}

Stoner::RHI::ERHICommandBufferState FVulkanCommandBuffer::GetState() const noexcept { return State; }
Stoner::RHI::ERHIQueueType FVulkanCommandBuffer::GetCompatibleQueueType() const noexcept { return QueueType; }
Stoner::Core::uint32 FVulkanCommandBuffer::GetRecordedCommandCount() const noexcept { return static_cast<Stoner::Core::uint32>(Commands.size()); }
const Stoner::Core::TArray<FVulkanRecordedCommand>& FVulkanCommandBuffer::GetRecordedCommands() const noexcept { return Commands; }
bool FVulkanCommandBuffer::IsValid() const noexcept
{
    return bValid && Owner && Owner->bActive;
}

bool FVulkanCommandBuffer::BelongsTo(
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept
{
    return IsValid() && InOwner && Owner == InOwner;
}
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
    BoundGraphicsPipeline.reset();
    BoundComputePipeline.reset();
    BoundVertexBuffer.reset();
    BoundIndexBuffer.reset();
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
    BoundGraphicsPipeline.reset();
    BoundComputePipeline.reset();
    BoundVertexBuffer.reset();
    BoundIndexBuffer.reset();
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
    if (!HasCompatibleGraphicsPipeline())
    {
        MarkRecordingDiagnostic("draw recorded with missing or invalid graphics pipeline binding");
    }
    else
    {
        MarkRecordingDiagnostic("draw recorded with compatible graphics pipeline binding");
    }
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::Draw;
    Command.A = VertexCount;
    Command.B = InstanceCount;
    AppendCommand(std::move(Command));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordDrawIndexed(
    const Stoner::RHI::FRHIIndexedDrawArguments& Arguments)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics || !HasActiveRenderPass() || !Stoner::RHI::IsValidRHIIndexedDrawArguments(Arguments))
    {
        MarkRecordingDiagnostic("indexed draw rejected; requires graphics recording inside render pass");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!HasCompatibleGraphicsPipeline())
    {
        MarkRecordingDiagnostic("indexed draw recorded with missing or invalid graphics pipeline binding");
    }
    else
    {
        MarkRecordingDiagnostic("indexed draw recorded with compatible graphics pipeline binding");
    }
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::DrawIndexed;
    Command.A = Arguments.IndexCount;
    Command.B = Arguments.InstanceCount;
    Command.C = Arguments.FirstIndex;
    Command.D = Arguments.VertexOffset;
    Command.E = Arguments.FirstInstance;
    Command.IndexedDraw = Arguments;
    AppendCommand(std::move(Command));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordDrawIndexed(
    Stoner::Core::uint32 IndexCount,
    Stoner::Core::uint32 InstanceCount,
    Stoner::Core::uint32 FirstInstance)
{
    return RecordDrawIndexed(
        {IndexCount, InstanceCount, 0, 0, FirstInstance});
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordDispatch(Stoner::Core::uint32 GroupCountX, Stoner::Core::uint32 GroupCountY, Stoner::Core::uint32 GroupCountZ)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsComputeCompatible() || GroupCountX == 0 || GroupCountY == 0 || GroupCountZ == 0)
    {
        MarkRecordingDiagnostic("dispatch rejected; requires compute-compatible recording state");
        return QueueType == Stoner::RHI::ERHIQueueType::Transfer ? Stoner::RHI::ERHIResult::Unsupported : Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!HasCompatibleComputePipeline())
    {
        MarkRecordingDiagnostic("dispatch recorded with missing or invalid compute pipeline binding");
    }
    else
    {
        MarkRecordingDiagnostic("dispatch recorded with compatible compute pipeline binding");
    }
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::Dispatch;
    Command.A = GroupCountX;
    Command.B = GroupCountY;
    Command.C = GroupCountZ;
    AppendCommand(std::move(Command));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::BindGraphicsPipeline(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline>& Pipeline)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics || !HasActiveRenderPass())
    {
        MarkRecordingDiagnostic("graphics pipeline binding rejected by recording queue or render pass scope");
        if (Diagnostics)
        {
            MarkPipelineBinding(*Diagnostics, "graphics pipeline binding rejected by recording queue or render pass scope");
        }
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto VulkanPipeline =
        std::dynamic_pointer_cast<FVulkanGraphicsPipeline>(Pipeline);
    if (!VulkanPipeline ||
        !VulkanPipeline->BelongsTo(Owner) ||
        !VulkanPipeline->HasValidDependencies())
    {
        MarkRecordingDiagnostic(
            "graphics pipeline binding rejected by ownership, lifecycle, or dependency state");
        if (Diagnostics)
        {
            MarkPipelineBinding(
                *Diagnostics,
                "graphics pipeline binding rejected by ownership, lifecycle, or dependency state");
        }
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    BoundGraphicsPipeline = Pipeline;
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::BindGraphicsPipeline;
    Command.GraphicsPipeline = Pipeline;
    AppendCommand(std::move(Command));
    MarkRecordingDiagnostic("graphics pipeline bound; deterministic fallback bind performs no real runtime execution");
    if (Diagnostics)
    {
        MarkPipelineBinding(*Diagnostics, "graphics pipeline bound; deterministic fallback bind performs no real runtime execution");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::BindComputePipeline(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIComputePipeline>& Pipeline)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsComputeCompatible())
    {
        MarkRecordingDiagnostic("compute pipeline binding rejected by recording queue");
        if (Diagnostics)
        {
            MarkPipelineBinding(*Diagnostics, "compute pipeline binding rejected by recording queue");
        }
        return QueueType == Stoner::RHI::ERHIQueueType::Transfer ? Stoner::RHI::ERHIResult::Unsupported : Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto VulkanPipeline =
        std::dynamic_pointer_cast<FVulkanComputePipeline>(Pipeline);
    if (!VulkanPipeline ||
        !VulkanPipeline->BelongsTo(Owner) ||
        !VulkanPipeline->HasValidDependencies())
    {
        MarkRecordingDiagnostic(
            "compute pipeline binding rejected by ownership, lifecycle, or dependency state");
        if (Diagnostics)
        {
            MarkPipelineBinding(
                *Diagnostics,
                "compute pipeline binding rejected by ownership, lifecycle, or dependency state");
        }
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    BoundComputePipeline = Pipeline;
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::BindComputePipeline;
    Command.ComputePipeline = Pipeline;
    AppendCommand(std::move(Command));
    MarkRecordingDiagnostic("compute pipeline bound; deterministic fallback bind performs no real runtime execution");
    if (Diagnostics)
    {
        MarkPipelineBinding(*Diagnostics, "compute pipeline bound; deterministic fallback bind performs no real runtime execution");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordBarrier()
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success)
    {
        MarkRecordingDiagnostic("generic barrier rejected outside recording");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::Barrier;
    AppendCommand(std::move(Command));
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
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::Barrier;
    Command.A = static_cast<Stoner::Core::uint64>(Barrier.Before);
    Command.B = static_cast<Stoner::Core::uint64>(Barrier.After);
    Command.Barrier = Barrier;
    AppendCommand(std::move(Command));
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
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::BufferCopy;
    Command.A = Range.SourceOffsetBytes;
    Command.B = Range.DestinationOffsetBytes;
    Command.C = Range.SizeBytes;
    Command.BufferA = Source;
    Command.BufferB = Destination;
    Command.BufferCopy = Range;
    AppendCommand(std::move(Command));
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
    if (!TexturesAreCopyCompatible(Source, Destination))
    {
        MarkRecordingDiagnostic("texture copy rejected by incompatible dimension format or sample count");
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
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::TextureCopy;
    Command.A = Region.Width;
    Command.B = Region.Height;
    Command.C = Region.Depth;
    Command.TextureA = Source;
    Command.TextureB = Destination;
    Command.TextureCopy = Region;
    AppendCommand(std::move(Command));
    MarkRecordingDiagnostic("texture copy recorded");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::RecordTextureToBufferCopy(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Source,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Destination,
    Stoner::RHI::FRHITextureBufferCopyRegion Region)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !IsTransferCompatible())
    {
        MarkRecordingDiagnostic("texture-to-buffer copy rejected by queue capability");
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    if (!TextureRegionFits(Source, Region.SourceMipLevel, Region.SourceArrayLayer, Region.SourceX, Region.SourceY,
            Region.SourceZ, Region.Width, Region.Height, Region.Depth) ||
        !IsValidResource(Destination) ||
        !Stoner::RHI::HasRHIFlag(Source->GetUsage(), Stoner::RHI::ERHITextureUsage::CopySource) ||
        !Stoner::RHI::HasRHIFlag(Destination->GetUsage(), Stoner::RHI::ERHIBufferUsage::CopyDestination))
    {
        MarkRecordingDiagnostic("texture-to-buffer copy rejected by resource lifecycle region or usage");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Source->GetDesc().SampleCount !=
            Stoner::RHI::ERHISampleCount::One ||
        !Stoner::RHI::GetRHIFormatInfo(
             Source->GetFormat()).IsValid())
    {
        MarkRecordingDiagnostic("texture-to-buffer copy rejected by format or sample count");
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    const Stoner::Core::uint64 RowTexels =
        Region.DestinationRowLengthTexels == 0
        ? Region.Width
        : Region.DestinationRowLengthTexels;
    const Stoner::Core::uint64 ImageRows =
        Region.DestinationImageHeightTexels == 0
        ? Region.Height
        : Region.DestinationImageHeightTexels;
    if (RowTexels < Region.Width || ImageRows < Region.Height)
    {
        MarkRecordingDiagnostic("texture-to-buffer copy rejected by destination strides");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    Stoner::Core::uint64 RequiredBytes = 0;
    if (!Stoner::RHI::TryGetRHITextureBufferCopyByteSize(
            Region, Source->GetFormat(), RequiredBytes))
    {
        MarkRecordingDiagnostic("texture-to-buffer copy footprint is not representable");
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (!BufferRangeFits(Destination, Region.DestinationOffsetBytes, RequiredBytes))
    {
        MarkRecordingDiagnostic("texture-to-buffer copy rejected by destination range");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::TextureToBufferCopy;
    Command.A = Region.DestinationOffsetBytes;
    Command.B = RequiredBytes;
    Command.C = Region.Width;
    Command.TextureA = Source;
    Command.BufferA = Destination;
    Command.TextureToBufferCopy = Region;
    AppendCommand(std::move(Command));
    MarkRecordingDiagnostic("texture-to-buffer copy recorded");
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
    Stoner::RHI::FRHIRenderPassClearValues ClearValues;
    for (const Stoner::RHI::FRHIRenderPassAttachmentDesc& Attachment : RenderPass->GetDesc().Attachments)
    {
        if (Attachment.LoadOp == Stoner::RHI::ERHIAttachmentLoadOp::Clear &&
            Attachment.Role == Stoner::RHI::ERHIAttachmentRole::Color)
        {
            ClearValues.Colors.push_back({});
        }
    }
    ActiveRenderPass = RenderPass;
    ActiveFramebuffer = Framebuffer;
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::BeginRenderPass;
    Command.A = Framebuffer->GetWidth();
    Command.B = Framebuffer->GetHeight();
    Command.C = Framebuffer->GetAttachmentCount();
    Command.RenderPass = RenderPass;
    Command.Framebuffer = Framebuffer;
    Command.ClearValues = std::move(ClearValues);
    AppendCommand(std::move(Command));
    MarkRecordingDiagnostic("render pass scope began");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::BeginRenderPass(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass>& RenderPass,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer>& Framebuffer,
    const Stoner::RHI::FRHIRenderPassClearValues& ClearValues)
{
    if (!RenderPass || !ClearValuesMatch(*RenderPass, ClearValues))
    {
        MarkRecordingDiagnostic("begin render pass rejected by explicit clear-value compatibility");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const Stoner::RHI::ERHIResult Result = BeginRenderPass(RenderPass, Framebuffer);
    if (Result == Stoner::RHI::ERHIResult::Success)
    {
        Commands.back().ClearValues = ClearValues;
    }
    return Result;
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
    BoundGraphicsPipeline.reset();
    BoundComputePipeline.reset();
    BoundIndexBuffer.reset();
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::EndRenderPass;
    AppendCommand(std::move(Command));
    MarkRecordingDiagnostic("render pass scope ended");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::BindVertexBuffer(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer, Stoner::Core::uint64 OffsetBytes)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics ||
        !HasActiveRenderPass() || !IsValidResource(Buffer) || !Stoner::RHI::HasRHIFlag(Buffer->GetUsage(), Stoner::RHI::ERHIBufferUsage::Vertex) ||
        OffsetBytes >= Buffer->GetSizeInBytes())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    BoundVertexBuffer = Buffer;
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::BindVertexBuffer;
    Command.A = OffsetBytes;
    Command.BufferA = Buffer;
    AppendCommand(std::move(Command));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::BindIndexBuffer(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer,
    Stoner::RHI::ERHIIndexType IndexType,
    Stoner::Core::uint64 OffsetBytes)
{
    const Stoner::Core::uint64 Alignment = Stoner::RHI::GetRHIIndexTypeSize(IndexType);
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics ||
        !HasActiveRenderPass() || !IsValidResource(Buffer) ||
        !Stoner::RHI::HasRHIFlag(Buffer->GetUsage(), Stoner::RHI::ERHIBufferUsage::Index) ||
        OffsetBytes >= Buffer->GetSizeInBytes() || OffsetBytes % Alignment != 0)
    {
        MarkRecordingDiagnostic("index buffer binding rejected by state lifecycle usage range or alignment");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    BoundIndexBuffer = Buffer;
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::BindIndexBuffer;
    Command.A = OffsetBytes;
    Command.B = Alignment;
    Command.BufferA = Buffer;
    Command.IndexType = IndexType;
    AppendCommand(std::move(Command));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::BindDescriptorSet(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDescriptorSet>& DescriptorSet)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || QueueType != Stoner::RHI::ERHIQueueType::Graphics ||
        !HasActiveRenderPass() || !DescriptorSet ||
        DescriptorSet->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        !DescriptorSet->GetPipelineLayout())
    {
        MarkRecordingDiagnostic("descriptor set binding rejected by state lifecycle or layout");
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::BindDescriptorSet;
    Command.A = DescriptorSet->GetSetIndex();
    Command.B = DescriptorSet->GetBoundResourceCount();
    Command.DescriptorSet = DescriptorSet;
    AppendCommand(std::move(Command));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::SetViewport(const Stoner::RHI::FRHIViewport& Viewport)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !HasActiveRenderPass() ||
        !(Viewport.Width > 0.0f) || !(Viewport.Height > 0.0f) || Viewport.MinDepth < 0.0f ||
        Viewport.MaxDepth > 1.0f || Viewport.MinDepth > Viewport.MaxDepth)
        return Stoner::RHI::ERHIResult::InvalidState;
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::SetViewport;
    Command.A = static_cast<Stoner::Core::uint64>(Viewport.Width);
    Command.B = static_cast<Stoner::Core::uint64>(Viewport.Height);
    Command.Viewport = Viewport;
    AppendCommand(std::move(Command));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanCommandBuffer::SetScissor(const Stoner::RHI::FRHIScissorRect& Scissor)
{
    if (ValidateRecordingState() != Stoner::RHI::ERHIResult::Success || !HasActiveRenderPass() || Scissor.Width == 0 || Scissor.Height == 0)
        return Stoner::RHI::ERHIResult::InvalidState;
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::SetScissor;
    Command.A = Scissor.Width;
    Command.B = Scissor.Height;
    Command.Scissor = Scissor;
    AppendCommand(std::move(Command));
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
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::UploadSchedule;
    Command.A = Range.OffsetBytes;
    Command.B = Range.SizeBytes;
    AppendCommand(std::move(Command));
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
    FVulkanRecordedCommand Command;
    Command.Type = Stoner::RHI::ERHISymbolicCommandType::UploadSchedule;
    Command.A = Region.Width;
    Command.B = Region.Height;
    Command.C = Region.Depth;
    AppendCommand(std::move(Command));
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
    BoundGraphicsPipeline.reset();
    BoundComputePipeline.reset();
    Diagnostics = nullptr;
}

bool FVulkanCommandBuffer::IsTransferCompatible() const noexcept
{
    return QueueType == Stoner::RHI::ERHIQueueType::Graphics || QueueType == Stoner::RHI::ERHIQueueType::Compute || QueueType == Stoner::RHI::ERHIQueueType::Transfer;
}

bool FVulkanCommandBuffer::IsComputeCompatible() const noexcept
{
    return QueueType == Stoner::RHI::ERHIQueueType::Graphics || QueueType == Stoner::RHI::ERHIQueueType::Compute;
}

bool FVulkanCommandBuffer::HasCompatibleGraphicsPipeline() const noexcept
{
    const auto Pipeline =
        std::dynamic_pointer_cast<FVulkanGraphicsPipeline>(
            BoundGraphicsPipeline.lock());
    return Pipeline &&
        Pipeline->BelongsTo(Owner) &&
        Pipeline->HasValidDependencies() &&
        HasActiveRenderPass();
}

bool FVulkanCommandBuffer::HasCompatibleComputePipeline() const noexcept
{
    const auto Pipeline =
        std::dynamic_pointer_cast<FVulkanComputePipeline>(
            BoundComputePipeline.lock());
    return Pipeline &&
        Pipeline->BelongsTo(Owner) &&
        Pipeline->HasValidDependencies() &&
        IsComputeCompatible();
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
