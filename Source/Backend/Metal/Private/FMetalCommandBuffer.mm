#include "FMetalCommandBuffer.h"

#include "FMetalBuffer.h"
#include "FMetalComputePipeline.h"
#include "FMetalDescriptorSet.h"
#include "FMetalFramebuffer.h"
#include "FMetalFailureInjector.h"
#include "FMetalGraphicsPipeline.h"
#include "FMetalRenderPass.h"
#include "FMetalTexture.h"

#include <cmath>
#include <new>

namespace Stoner::Backend::Metal::Private
{
namespace
{

bool BufferRangeFits(
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
    Core::uint64 Offset,
    Core::uint64 Size) noexcept
{
    return Buffer && Size > 0 && Offset <= Buffer->GetSizeInBytes() &&
        Size <= Buffer->GetSizeInBytes() - Offset;
}

bool TextureRegionFits(
    const Core::TSharedPtr<RHI::IRHITexture>& Texture,
    Core::uint32 Mip, Core::uint32 Layer,
    Core::uint32 X, Core::uint32 Y, Core::uint32 Z,
    Core::uint32 Width, Core::uint32 Height, Core::uint32 Depth) noexcept
{
    return Texture && RHI::IsRHITextureRegionValid(
        Texture->GetDesc(), Mip, Layer, X, Y, Z, Width, Height, Depth);
}

bool ClearValuesMatch(
    const RHI::IRHIRenderPass& RenderPass,
    const RHI::FRHIRenderPassClearValues& Values) noexcept
{
    Core::uint32 ColorCount = 0;
    bool bDepthClear = false;
    for (const auto& Attachment : RenderPass.GetDesc().Attachments)
    {
        if (Attachment.LoadOp != RHI::ERHIAttachmentLoadOp::Clear) continue;
        if (Attachment.Role == RHI::ERHIAttachmentRole::Color) ++ColorCount;
        else bDepthClear = true;
    }
    return Values.Colors.size() == ColorCount &&
        (!bDepthClear || (Values.Depth >= 0.0f && Values.Depth <= 1.0f));
}

} // namespace

FMetalCommandBuffer::FMetalCommandBuffer(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::ERHIQueueType QueueType) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Command),
      QueueType_(QueueType)
{
}

FMetalCommandBuffer::~FMetalCommandBuffer()
{
    std::lock_guard Lock(Mutex_);
    (void)InvalidateObject();
    Records_.clear();
    ClearRecordingState();
}

RHI::ERHICommandBufferState FMetalCommandBuffer::GetState() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return State_;
}

RHI::ERHIQueueType FMetalCommandBuffer::GetCompatibleQueueType()
    const noexcept { return QueueType_; }

Core::uint32 FMetalCommandBuffer::GetRecordedCommandCount() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return static_cast<Core::uint32>(Records_.size());
}

bool FMetalCommandBuffer::IsRecording() const noexcept
{
    return IsCompatible(GetOwner()) &&
        State_ == RHI::ERHICommandBufferState::Recording;
}
bool FMetalCommandBuffer::SupportsTransfer() const noexcept
{
    return QueueType_ == RHI::ERHIQueueType::Graphics ||
        QueueType_ == RHI::ERHIQueueType::Compute ||
        QueueType_ == RHI::ERHIQueueType::Transfer;
}
bool FMetalCommandBuffer::SupportsCompute() const noexcept
{
    return QueueType_ == RHI::ERHIQueueType::Graphics ||
        QueueType_ == RHI::ERHIQueueType::Compute;
}

RHI::ERHIResult FMetalCommandBuffer::Append(FMetalCommandRecord Record) noexcept
{
    try
    {
        Records_.push_back(std::move(Record));
        return RHI::ERHIResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        return RHI::ERHIResult::Failed;
    }
    catch (const std::length_error&)
    {
        return RHI::ERHIResult::Failed;
    }
}

void FMetalCommandBuffer::ClearRecordingState() noexcept
{
    ActiveRenderPass_.reset();
    ActiveFramebuffer_.reset();
    BoundGraphicsPipeline_.reset();
    BoundComputePipeline_.reset();
    BoundVertexBuffer_.reset();
    BoundIndexBuffer_.reset();
}

RHI::ERHIResult FMetalCommandBuffer::Begin()
{
    std::lock_guard Lock(Mutex_);
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::CommandRecording))
    {
        GetOwner()->RecordDiagnostic(
            Core::FString("BeginCommandBuffer"), Core::FString("command"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::CommandRecording)),
            0, 0, {}, Core::FString("resettable"));
        return RHI::ERHIResult::Failed;
    }
    if (!IsCompatible(GetOwner()) ||
        (State_ != RHI::ERHICommandBufferState::Idle &&
         State_ != RHI::ERHICommandBufferState::Resettable))
        return RHI::ERHIResult::InvalidState;
    Records_.clear();
    ClearRecordingState();
    State_ = RHI::ERHICommandBufferState::Recording;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalCommandBuffer::End()
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || ActiveRenderPass_)
        return RHI::ERHIResult::InvalidState;
    State_ = RHI::ERHICommandBufferState::Completed;
    ClearRecordingState();
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalCommandBuffer::Reset()
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) ||
        State_ == RHI::ERHICommandBufferState::Recording ||
        State_ == RHI::ERHICommandBufferState::Submitted)
        return RHI::ERHIResult::InvalidState;
    Records_.clear();
    ClearRecordingState();
    State_ = RHI::ERHICommandBufferState::Idle;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalCommandBuffer::RecordDraw(
    Core::uint32 VertexCount, Core::uint32 InstanceCount)
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || QueueType_ != RHI::ERHIQueueType::Graphics ||
        !ActiveRenderPass_ || !BoundGraphicsPipeline_ || !BoundVertexBuffer_ ||
        VertexCount == 0 || InstanceCount == 0)
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::Draw;
    Record.A = VertexCount;
    Record.B = InstanceCount;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordDrawIndexed(
    const RHI::FRHIIndexedDrawArguments& Arguments)
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || QueueType_ != RHI::ERHIQueueType::Graphics ||
        !ActiveRenderPass_ || !BoundGraphicsPipeline_ || !BoundVertexBuffer_ ||
        !BoundIndexBuffer_ || !RHI::IsValidRHIIndexedDrawArguments(Arguments))
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::DrawIndexed;
    Record.IndexedDraw = Arguments;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordDrawIndexed(
    Core::uint32 IndexCount, Core::uint32 InstanceCount,
    Core::uint32 FirstInstance)
{
    return RecordDrawIndexed(
        {IndexCount, InstanceCount, 0, 0, FirstInstance});
}

RHI::ERHIResult FMetalCommandBuffer::RecordDispatch(
    Core::uint32 X, Core::uint32 Y, Core::uint32 Z)
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || !SupportsCompute() || ActiveRenderPass_ ||
        !BoundComputePipeline_ || X == 0 || Y == 0 || Z == 0)
        return QueueType_ == RHI::ERHIQueueType::Transfer
            ? RHI::ERHIResult::Unsupported : RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::Dispatch;
    Record.A = X; Record.B = Y; Record.C = Z;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::BindGraphicsPipeline(
    const Core::TSharedPtr<RHI::IRHIGraphicsPipeline>& Pipeline)
{
    std::lock_guard Lock(Mutex_);
    const auto Native = std::dynamic_pointer_cast<FMetalGraphicsPipeline>(Pipeline);
    if (!IsRecording() || QueueType_ != RHI::ERHIQueueType::Graphics ||
        !ActiveRenderPass_ || !Native || !Native->IsCompatible(GetOwner()))
        return RHI::ERHIResult::InvalidState;
    BoundGraphicsPipeline_ = Pipeline;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::BindGraphicsPipeline;
    Record.GraphicsPipeline = Pipeline;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::BindComputePipeline(
    const Core::TSharedPtr<RHI::IRHIComputePipeline>& Pipeline)
{
    std::lock_guard Lock(Mutex_);
    const auto Native = std::dynamic_pointer_cast<FMetalComputePipeline>(Pipeline);
    if (!IsRecording() || !SupportsCompute() || ActiveRenderPass_ ||
        !Native || !Native->IsCompatible(GetOwner()))
        return QueueType_ == RHI::ERHIQueueType::Transfer
            ? RHI::ERHIResult::Unsupported : RHI::ERHIResult::InvalidState;
    BoundComputePipeline_ = Pipeline;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::BindComputePipeline;
    Record.ComputePipeline = Pipeline;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordBarrier()
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording()) return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::Barrier;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordBarrier(
    const RHI::FRHIResourceBarrierDesc& Barrier)
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || Barrier.Before == Barrier.After ||
        (static_cast<bool>(Barrier.Buffer) == static_cast<bool>(Barrier.Texture)))
        return RHI::ERHIResult::InvalidState;
    if (Barrier.Buffer)
    {
        const auto Native = std::dynamic_pointer_cast<FMetalBuffer>(Barrier.Buffer);
        if (!Native || !Native->IsCompatible(GetOwner()) ||
            (Barrier.RequiredBufferUsage != RHI::ERHIBufferUsage::None &&
             !RHI::HasRHIFlag(
                 Native->GetUsage(), Barrier.RequiredBufferUsage)))
            return RHI::ERHIResult::InvalidState;
    }
    else
    {
        const auto Native = std::dynamic_pointer_cast<FMetalTexture>(Barrier.Texture);
        if (!Native || !Native->IsCompatible(GetOwner()) ||
            (Barrier.RequiredTextureUsage != RHI::ERHITextureUsage::None &&
             !RHI::HasRHIFlag(
                 Native->GetUsage(), Barrier.RequiredTextureUsage)))
            return RHI::ERHIResult::InvalidState;
    }
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::Barrier;
    Record.Barrier = Barrier;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordBufferCopy(
    const Core::TSharedPtr<RHI::IRHIBuffer>& Source,
    const Core::TSharedPtr<RHI::IRHIBuffer>& Destination,
    RHI::FRHIBufferCopyRange Range)
{
    std::lock_guard Lock(Mutex_);
    const auto Src = std::dynamic_pointer_cast<FMetalBuffer>(Source);
    const auto Dst = std::dynamic_pointer_cast<FMetalBuffer>(Destination);
    if (!IsRecording() || !SupportsTransfer() || ActiveRenderPass_ ||
        !Src || !Dst || !Src->IsCompatible(GetOwner()) ||
        !Dst->IsCompatible(GetOwner()) ||
        !BufferRangeFits(Source, Range.SourceOffsetBytes, Range.SizeBytes) ||
        !BufferRangeFits(
            Destination, Range.DestinationOffsetBytes, Range.SizeBytes) ||
        !RHI::HasRHIFlag(Source->GetUsage(), RHI::ERHIBufferUsage::CopySource) ||
        !RHI::HasRHIFlag(
            Destination->GetUsage(), RHI::ERHIBufferUsage::CopyDestination))
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::BufferCopy;
    Record.BufferA = Source; Record.BufferB = Destination;
    Record.BufferCopy = Range;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordTextureCopy(
    const Core::TSharedPtr<RHI::IRHITexture>& Source,
    const Core::TSharedPtr<RHI::IRHITexture>& Destination,
    RHI::FRHITextureCopyRegion Region)
{
    std::lock_guard Lock(Mutex_);
    const auto Src = std::dynamic_pointer_cast<FMetalTexture>(Source);
    const auto Dst = std::dynamic_pointer_cast<FMetalTexture>(Destination);
    if (!IsRecording() || !SupportsTransfer() || ActiveRenderPass_ ||
        !Src || !Dst || !Src->IsCompatible(GetOwner()) ||
        !Dst->IsCompatible(GetOwner()) ||
        Source->GetDimension() != Destination->GetDimension() ||
        Source->GetFormat() != Destination->GetFormat() ||
        !TextureRegionFits(Source, Region.SourceMipLevel,
            Region.SourceArrayLayer, Region.SourceX, Region.SourceY,
            Region.SourceZ, Region.Width, Region.Height, Region.Depth) ||
        !TextureRegionFits(Destination, Region.DestinationMipLevel,
            Region.DestinationArrayLayer, Region.DestinationX,
            Region.DestinationY, Region.DestinationZ,
            Region.Width, Region.Height, Region.Depth) ||
        !RHI::HasRHIFlag(Source->GetUsage(), RHI::ERHITextureUsage::CopySource) ||
        !RHI::HasRHIFlag(
            Destination->GetUsage(), RHI::ERHITextureUsage::CopyDestination))
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::TextureCopy;
    Record.TextureA = Source; Record.TextureB = Destination;
    Record.TextureCopy = Region;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordLayoutTransition(
    const RHI::FRHIResourceBarrierDesc& Transition)
{
    const auto Result = RecordBarrier(Transition);
    if (Result != RHI::ERHIResult::Success) return Result;
    std::lock_guard Lock(Mutex_);
    Records_.back().Type = RHI::ERHISymbolicCommandType::LayoutTransition;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalCommandBuffer::BeginRenderPass(
    const Core::TSharedPtr<RHI::IRHIRenderPass>& RenderPass,
    const Core::TSharedPtr<RHI::IRHIFramebuffer>& Framebuffer)
{
    RHI::FRHIRenderPassClearValues Values;
    if (RenderPass)
    {
        for (const auto& Attachment : RenderPass->GetDesc().Attachments)
            if (Attachment.Role == RHI::ERHIAttachmentRole::Color &&
                Attachment.LoadOp == RHI::ERHIAttachmentLoadOp::Clear)
                Values.Colors.push_back({});
    }
    return BeginRenderPass(RenderPass, Framebuffer, Values);
}

RHI::ERHIResult FMetalCommandBuffer::BeginRenderPass(
    const Core::TSharedPtr<RHI::IRHIRenderPass>& RenderPass,
    const Core::TSharedPtr<RHI::IRHIFramebuffer>& Framebuffer,
    const RHI::FRHIRenderPassClearValues& ClearValues)
{
    std::lock_guard Lock(Mutex_);
    const auto Pass = std::dynamic_pointer_cast<FMetalRenderPass>(RenderPass);
    const auto Frame = std::dynamic_pointer_cast<FMetalFramebuffer>(Framebuffer);
    if (!IsRecording() || QueueType_ != RHI::ERHIQueueType::Graphics ||
        ActiveRenderPass_ || !Pass || !Frame ||
        !Pass->IsCompatible(GetOwner()) || !Frame->IsCompatible(GetOwner()) ||
        Frame->GetRenderPass().get() != RenderPass.get() ||
        !ClearValuesMatch(*RenderPass, ClearValues))
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::BeginRenderPass;
    Record.RenderPass = RenderPass; Record.Framebuffer = Framebuffer;
    Record.ClearValues = ClearValues;
    const auto Result = Append(std::move(Record));
    if (Result == RHI::ERHIResult::Success)
    {
        ActiveRenderPass_ = RenderPass;
        ActiveFramebuffer_ = Framebuffer;
    }
    return Result;
}

RHI::ERHIResult FMetalCommandBuffer::EndRenderPass()
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || !ActiveRenderPass_)
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::EndRenderPass;
    const auto Result = Append(std::move(Record));
    if (Result == RHI::ERHIResult::Success)
    {
        ActiveRenderPass_.reset(); ActiveFramebuffer_.reset();
        BoundGraphicsPipeline_.reset(); BoundVertexBuffer_.reset();
        BoundIndexBuffer_.reset();
    }
    return Result;
}

RHI::ERHIResult FMetalCommandBuffer::BindVertexBuffer(
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer, Core::uint64 Offset)
{
    std::lock_guard Lock(Mutex_);
    const auto Native = std::dynamic_pointer_cast<FMetalBuffer>(Buffer);
    if (!IsRecording() || !ActiveRenderPass_ || !Native ||
        !Native->IsCompatible(GetOwner()) || Offset >= Buffer->GetSizeInBytes() ||
        !RHI::HasRHIFlag(Buffer->GetUsage(), RHI::ERHIBufferUsage::Vertex))
        return RHI::ERHIResult::InvalidState;
    BoundVertexBuffer_ = Buffer;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::BindVertexBuffer;
    Record.BufferA = Buffer; Record.A = Offset;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::BindIndexBuffer(
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
    RHI::ERHIIndexType IndexType, Core::uint64 Offset)
{
    std::lock_guard Lock(Mutex_);
    const auto Native = std::dynamic_pointer_cast<FMetalBuffer>(Buffer);
    const Core::uint64 Alignment = RHI::GetRHIIndexTypeSize(IndexType);
    if (!IsRecording() || !ActiveRenderPass_ || !Native ||
        !Native->IsCompatible(GetOwner()) || Offset >= Buffer->GetSizeInBytes() ||
        Offset % Alignment != 0 ||
        !RHI::HasRHIFlag(Buffer->GetUsage(), RHI::ERHIBufferUsage::Index))
        return RHI::ERHIResult::InvalidState;
    BoundIndexBuffer_ = Buffer;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::BindIndexBuffer;
    Record.BufferA = Buffer; Record.A = Offset; Record.IndexType = IndexType;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::BindDescriptorSet(
    const Core::TSharedPtr<RHI::IRHIDescriptorSet>& DescriptorSet)
{
    std::lock_guard Lock(Mutex_);
    const auto Native = std::dynamic_pointer_cast<FMetalDescriptorSet>(DescriptorSet);
    Core::TSharedPtr<RHI::IRHIPipelineLayout> Expected;
    if (BoundGraphicsPipeline_) Expected = BoundGraphicsPipeline_->GetPipelineLayout();
    else if (BoundComputePipeline_) Expected = BoundComputePipeline_->GetPipelineLayout();
    if (!IsRecording() || !Native || !Native->IsCompatible(GetOwner()) ||
        !Expected || DescriptorSet->GetPipelineLayout().get() != Expected.get())
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::BindDescriptorSet;
    Record.DescriptorSet = DescriptorSet;
    Record.DescriptorSetIndex = Native->GetSetIndex();
    try
    {
        Record.DescriptorSnapshot = Native->Snapshot();
    }
    catch (const std::bad_alloc&)
    {
        return RHI::ERHIResult::Failed;
    }
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::RecordTextureToBufferCopy(
    const Core::TSharedPtr<RHI::IRHITexture>& Source,
    const Core::TSharedPtr<RHI::IRHIBuffer>& Destination,
    RHI::FRHITextureBufferCopyRegion Region)
{
    std::lock_guard Lock(Mutex_);
    const auto Src = std::dynamic_pointer_cast<FMetalTexture>(Source);
    const auto Dst = std::dynamic_pointer_cast<FMetalBuffer>(Destination);
    Core::uint64 ByteSize = 0;
    if (!IsRecording() || !SupportsTransfer() || ActiveRenderPass_ ||
        !Src || !Dst || !Src->IsCompatible(GetOwner()) ||
        !Dst->IsCompatible(GetOwner()) ||
        !TextureRegionFits(Source, Region.SourceMipLevel,
            Region.SourceArrayLayer, Region.SourceX, Region.SourceY,
            Region.SourceZ, Region.Width, Region.Height, Region.Depth) ||
        !RHI::TryGetRHITextureBufferCopyByteSize(
            Region, Source->GetFormat(), ByteSize) ||
        !BufferRangeFits(Destination, Region.DestinationOffsetBytes, ByteSize) ||
        !RHI::HasRHIFlag(Source->GetUsage(), RHI::ERHITextureUsage::CopySource) ||
        !RHI::HasRHIFlag(
            Destination->GetUsage(), RHI::ERHIBufferUsage::CopyDestination))
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::TextureToBufferCopy;
    Record.TextureA = Source; Record.BufferA = Destination;
    Record.TextureToBufferCopy = Region;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::SetViewport(
    const RHI::FRHIViewport& Viewport)
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || !ActiveRenderPass_ ||
        !std::isfinite(Viewport.X) || !std::isfinite(Viewport.Y) ||
        !std::isfinite(Viewport.Width) || !std::isfinite(Viewport.Height) ||
        Viewport.Width <= 0.0f || Viewport.Height <= 0.0f ||
        Viewport.MinDepth < 0.0f || Viewport.MaxDepth > 1.0f ||
        Viewport.MinDepth > Viewport.MaxDepth)
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::SetViewport;
    Record.Viewport = Viewport;
    return Append(std::move(Record));
}

RHI::ERHIResult FMetalCommandBuffer::SetScissor(
    const RHI::FRHIScissorRect& Scissor)
{
    std::lock_guard Lock(Mutex_);
    if (!IsRecording() || !ActiveRenderPass_ ||
        Scissor.Width == 0 || Scissor.Height == 0)
        return RHI::ERHIResult::InvalidState;
    FMetalCommandRecord Record;
    Record.Type = RHI::ERHISymbolicCommandType::SetScissor;
    Record.Scissor = Scissor;
    return Append(std::move(Record));
}

bool FMetalCommandBuffer::IsCompatibleWith(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept
{
    return IsCompatible(Owner);
}

bool FMetalCommandBuffer::PrepareSubmission(
    Core::TArray<FMetalCommandRecord>& OutRecords) noexcept
{
    std::lock_guard Lock(Mutex_);
    if (!IsCompatible(GetOwner()) ||
        State_ != RHI::ERHICommandBufferState::Completed || Records_.empty())
        return false;
    try
    {
        OutRecords = Records_;
    }
    catch (const std::bad_alloc&)
    {
        OutRecords.clear();
        return false;
    }
    catch (const std::length_error&)
    {
        OutRecords.clear();
        return false;
    }
    State_ = RHI::ERHICommandBufferState::Submitted;
    return true;
}

void FMetalCommandBuffer::CompleteSubmission() noexcept
{
    std::lock_guard Lock(Mutex_);
    if (State_ == RHI::ERHICommandBufferState::Submitted)
        State_ = RHI::ERHICommandBufferState::Resettable;
}

} // namespace Stoner::Backend::Metal::Private
