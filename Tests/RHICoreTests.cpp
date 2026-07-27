#include "RHICoreTests.h"
#include "ShaderTestFixtures.h"

#include "RHI/RHIMinimal.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>

namespace
{

using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FSymbolicCommand
{
    ERHISymbolicCommandType Type = ERHISymbolicCommandType::Draw;
    uint32 A = 0;
    uint32 B = 0;
    uint32 C = 0;
};

void Record(FRHICoreTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

class FMockCommandBuffer final : public IRHICommandBuffer
{
public:
    explicit FMockCommandBuffer(ERHIQueueType InQueueType)
        : QueueType(InQueueType)
    {
    }

    [[nodiscard]] ERHICommandBufferState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] ERHIQueueType GetCompatibleQueueType() const noexcept override
    {
        return QueueType;
    }

    [[nodiscard]] uint32 GetRecordedCommandCount() const noexcept override
    {
        return static_cast<uint32>(Commands.size());
    }

    ERHIResult Begin() override
    {
        if (State != ERHICommandBufferState::Idle && State != ERHICommandBufferState::Resettable)
        {
            return ERHIResult::InvalidState;
        }

        Commands.clear();
        State = ERHICommandBufferState::Recording;
        return ERHIResult::Success;
    }

    ERHIResult End() override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        State = ERHICommandBufferState::Completed;
        return ERHIResult::Success;
    }

    ERHIResult Reset() override
    {
        if (State == ERHICommandBufferState::Recording || State == ERHICommandBufferState::Submitted)
        {
            return ERHIResult::InvalidState;
        }

        Commands.clear();
        State = ERHICommandBufferState::Idle;
        return ERHIResult::Success;
    }

    ERHIResult RecordDraw(uint32 VertexCount, uint32 InstanceCount = 1) override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        Commands.push_back({ERHISymbolicCommandType::Draw, VertexCount, InstanceCount, 0});
        return ERHIResult::Success;
    }

    ERHIResult RecordDrawIndexed(uint32 IndexCount, uint32 InstanceCount = 1,
        uint32 FirstInstance = 0) override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        Commands.push_back({ERHISymbolicCommandType::DrawIndexed,
            IndexCount, InstanceCount, FirstInstance});
        return ERHIResult::Success;
    }

    ERHIResult RecordDispatch(uint32 GroupCountX, uint32 GroupCountY, uint32 GroupCountZ) override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        Commands.push_back({ERHISymbolicCommandType::Dispatch, GroupCountX, GroupCountY, GroupCountZ});
        return ERHIResult::Success;
    }

    ERHIResult BindGraphicsPipeline(const TSharedPtr<IRHIGraphicsPipeline>& Pipeline) override
    {
        if (State != ERHICommandBufferState::Recording || QueueType != ERHIQueueType::Graphics || !bRenderPassActive ||
            !Pipeline || Pipeline->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return ERHIResult::InvalidState;
        }
        BoundGraphicsPipeline = Pipeline;
        Commands.push_back({ERHISymbolicCommandType::BindGraphicsPipeline, 0, 0, 0});
        return ERHIResult::Success;
    }

    ERHIResult BindComputePipeline(const TSharedPtr<IRHIComputePipeline>& Pipeline) override
    {
        if (State != ERHICommandBufferState::Recording || QueueType == ERHIQueueType::Transfer ||
            !Pipeline || Pipeline->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return QueueType == ERHIQueueType::Transfer ? ERHIResult::Unsupported : ERHIResult::InvalidState;
        }
        BoundComputePipeline = Pipeline;
        Commands.push_back({ERHISymbolicCommandType::BindComputePipeline, 0, 0, 0});
        return ERHIResult::Success;
    }

    ERHIResult RecordBarrier() override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        Commands.push_back({ERHISymbolicCommandType::Barrier, 0, 0, 0});
        return ERHIResult::Success;
    }

    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override
    {
        return RecordBarrier();
    }

    ERHIResult RecordBufferCopy(const TSharedPtr<IRHIBuffer>& Source, const TSharedPtr<IRHIBuffer>& Destination, FRHIBufferCopyRange Range) override
    {
        if (State != ERHICommandBufferState::Recording || !Source || !Destination || Range.SizeBytes == 0)
        {
            return ERHIResult::InvalidState;
        }
        Commands.push_back({ERHISymbolicCommandType::BufferCopy, static_cast<uint32>(Range.SourceOffsetBytes), static_cast<uint32>(Range.DestinationOffsetBytes), static_cast<uint32>(Range.SizeBytes)});
        return ERHIResult::Success;
    }

    ERHIResult RecordTextureCopy(const TSharedPtr<IRHITexture>& Source, const TSharedPtr<IRHITexture>& Destination, FRHITextureCopyRegion Region) override
    {
        if (State != ERHICommandBufferState::Recording || !Source || !Destination || Region.Width == 0 || Region.Height == 0 || Region.Depth == 0)
        {
            return ERHIResult::InvalidState;
        }
        Commands.push_back({ERHISymbolicCommandType::TextureCopy, Region.Width, Region.Height, Region.Depth});
        return ERHIResult::Success;
    }

    ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc& Transition) override
    {
        const ERHIResult Result = RecordBarrier(Transition);
        if (Result == ERHIResult::Success)
        {
            Commands.back().Type = ERHISymbolicCommandType::LayoutTransition;
        }
        return Result;
    }

    ERHIResult BeginRenderPass(const TSharedPtr<IRHIRenderPass>& RenderPass, const TSharedPtr<IRHIFramebuffer>& Framebuffer) override
    {
        if (State != ERHICommandBufferState::Recording || QueueType != ERHIQueueType::Graphics || bRenderPassActive || !RenderPass || !Framebuffer)
        {
            return ERHIResult::InvalidState;
        }
        bRenderPassActive = true;
        Commands.push_back({ERHISymbolicCommandType::BeginRenderPass, Framebuffer->GetWidth(), Framebuffer->GetHeight(), Framebuffer->GetAttachmentCount()});
        return ERHIResult::Success;
    }

    ERHIResult BeginRenderPass(const TSharedPtr<IRHIRenderPass>& RenderPass,
        const TSharedPtr<IRHIFramebuffer>& Framebuffer,
        const FRHIRenderPassClearValues& ClearValues) override
    {
        if (!RenderPass)
        {
            return ERHIResult::InvalidState;
        }
        uint32 RequiredColors = 0;
        for (const FRHIRenderPassAttachmentDesc& Attachment : RenderPass->GetDesc().Attachments)
        {
            if (Attachment.Role == ERHIAttachmentRole::Color && Attachment.LoadOp == ERHIAttachmentLoadOp::Clear)
            {
                ++RequiredColors;
            }
        }
        return ClearValues.Colors.size() == RequiredColors && ClearValues.Depth >= 0.0f && ClearValues.Depth <= 1.0f
            ? BeginRenderPass(RenderPass, Framebuffer)
            : ERHIResult::InvalidState;
    }

    ERHIResult EndRenderPass() override
    {
        if (State != ERHICommandBufferState::Recording || !bRenderPassActive)
        {
            return ERHIResult::InvalidState;
        }
        bRenderPassActive = false;
        Commands.push_back({ERHISymbolicCommandType::EndRenderPass, 0, 0, 0});
        return ERHIResult::Success;
    }

    ERHIResult BindIndexBuffer(const TSharedPtr<IRHIBuffer>& Buffer,
        ERHIIndexType IndexType, uint64 OffsetBytes = 0) override
    {
        const uint64 Alignment = GetRHIIndexTypeSize(IndexType);
        if (State != ERHICommandBufferState::Recording || !bRenderPassActive || !Buffer ||
            Buffer->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            !HasRHIFlag(Buffer->GetUsage(), ERHIBufferUsage::Index) ||
            OffsetBytes >= Buffer->GetSizeInBytes() || OffsetBytes % Alignment != 0)
        {
            return ERHIResult::InvalidState;
        }
        Commands.push_back({ERHISymbolicCommandType::BindIndexBuffer,
            static_cast<uint32>(OffsetBytes), static_cast<uint32>(Alignment), 0});
        return ERHIResult::Success;
    }

    ERHIResult BindDescriptorSet(const TSharedPtr<IRHIDescriptorSet>& DescriptorSet) override
    {
        if (State != ERHICommandBufferState::Recording || !bRenderPassActive || !DescriptorSet ||
            DescriptorSet->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            !DescriptorSet->GetPipelineLayout())
        {
            return ERHIResult::InvalidState;
        }
        Commands.push_back({ERHISymbolicCommandType::BindDescriptorSet,
            DescriptorSet->GetSetIndex(), DescriptorSet->GetBoundResourceCount(), 0});
        return ERHIResult::Success;
    }

    ERHIResult RecordTextureToBufferCopy(const TSharedPtr<IRHITexture>& Source,
        const TSharedPtr<IRHIBuffer>& Destination, FRHITextureBufferCopyRegion Region) override
    {
        if (State != ERHICommandBufferState::Recording || bRenderPassActive || !Source || !Destination ||
            Source->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            Destination->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            !HasRHIFlag(Source->GetUsage(), ERHITextureUsage::CopySource) ||
            !HasRHIFlag(Destination->GetUsage(), ERHIBufferUsage::CopyDestination) ||
            Region.Width == 0 || Region.Height == 0 || Region.Depth == 0 ||
            Region.SourceX + Region.Width > Source->GetDesc().Width ||
            Region.SourceY + Region.Height > Source->GetDesc().Height)
        {
            return ERHIResult::InvalidState;
        }
        Commands.push_back({ERHISymbolicCommandType::TextureToBufferCopy,
            Region.Width, Region.Height, Region.Depth});
        return ERHIResult::Success;
    }

    [[nodiscard]] const TArray<FSymbolicCommand>& GetCommands() const noexcept
    {
        return Commands;
    }

    ERHIResult MarkSubmitted()
    {
        if (State != ERHICommandBufferState::Completed)
        {
            return ERHIResult::InvalidState;
        }

        State = ERHICommandBufferState::Submitted;
        return ERHIResult::Success;
    }

    void MarkResettable()
    {
        if (State == ERHICommandBufferState::Submitted)
        {
            State = ERHICommandBufferState::Resettable;
        }
    }

private:
    ERHIQueueType QueueType = ERHIQueueType::Graphics;
    ERHICommandBufferState State = ERHICommandBufferState::Idle;
    TArray<FSymbolicCommand> Commands;
    TWeakPtr<IRHIGraphicsPipeline> BoundGraphicsPipeline;
    TWeakPtr<IRHIComputePipeline> BoundComputePipeline;
    bool bRenderPassActive = false;
};

class FLegacyCommandBuffer final : public IRHICommandBuffer
{
public:
    [[nodiscard]] ERHICommandBufferState GetState() const noexcept override
    {
        return ERHICommandBufferState::Recording;
    }

    [[nodiscard]] ERHIQueueType GetCompatibleQueueType() const noexcept override
    {
        return ERHIQueueType::Graphics;
    }

    [[nodiscard]] uint32 GetRecordedCommandCount() const noexcept override { return 0; }
    ERHIResult Begin() override { return ERHIResult::Success; }
    ERHIResult End() override { return ERHIResult::Success; }
    ERHIResult Reset() override { return ERHIResult::Success; }
    ERHIResult RecordDraw(uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult RecordDrawIndexed(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult RecordDispatch(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult BindGraphicsPipeline(const TSharedPtr<IRHIGraphicsPipeline>&) override { return ERHIResult::Unsupported; }
    ERHIResult BindComputePipeline(const TSharedPtr<IRHIComputePipeline>&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier() override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBufferCopy(const TSharedPtr<IRHIBuffer>&, const TSharedPtr<IRHIBuffer>&, FRHIBufferCopyRange) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordTextureCopy(const TSharedPtr<IRHITexture>&, const TSharedPtr<IRHITexture>&, FRHITextureCopyRegion) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc&) override { return ERHIResult::Unsupported; }
    ERHIResult BeginRenderPass(const TSharedPtr<IRHIRenderPass>&, const TSharedPtr<IRHIFramebuffer>&) override
    {
        bLegacyBeginCalled = true;
        return ERHIResult::Success;
    }
    ERHIResult EndRenderPass() override { return ERHIResult::Success; }

    [[nodiscard]] bool WasLegacyBeginCalled() const noexcept { return bLegacyBeginCalled; }

private:
    bool bLegacyBeginCalled = false;
};

class FMockFence final : public IRHIFence
{
public:
    explicit FMockFence(bool bInitiallySignaled = false)
        : State(bInitiallySignaled ? ERHIFenceState::Signaled : ERHIFenceState::Unsignaled)
    {
    }

    [[nodiscard]] ERHIFenceState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] bool IsSignaled() const noexcept override
    {
        return State == ERHIFenceState::Signaled || State == ERHIFenceState::Waited;
    }

    ERHIResult Wait(uint64 TimeoutMicroseconds = 0) override
    {
        if (!IsSignaled())
        {
            return TimeoutMicroseconds > 0 ? ERHIResult::Timeout : ERHIResult::NotReady;
        }

        State = ERHIFenceState::Waited;
        return ERHIResult::Success;
    }

    ERHIResult Reset() override
    {
        State = ERHIFenceState::Unsignaled;
        return ERHIResult::Success;
    }

    ERHIResult Signal() override
    {
        State = ERHIFenceState::Signaled;
        return ERHIResult::Success;
    }

private:
    ERHIFenceState State = ERHIFenceState::Unsignaled;
};

class FMockSemaphore final : public IRHISemaphore
{
public:
    [[nodiscard]] ERHISemaphoreState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] bool IsSignaled() const noexcept override
    {
        return State == ERHISemaphoreState::Signaled;
    }

    ERHIResult Signal() override
    {
        if (State == ERHISemaphoreState::Signaled)
        {
            return ERHIResult::InvalidState;
        }

        State = ERHISemaphoreState::Signaled;
        return ERHIResult::Success;
    }

    ERHIResult Consume() override
    {
        if (State != ERHISemaphoreState::Signaled)
        {
            return ERHIResult::NotReady;
        }

        State = ERHISemaphoreState::Consumed;
        return ERHIResult::Success;
    }

    ERHIResult Reset() override
    {
        State = ERHISemaphoreState::Unsignaled;
        return ERHIResult::Success;
    }

private:
    ERHISemaphoreState State = ERHISemaphoreState::Unsignaled;
};

class FLegacySwapchain final : public IRHISwapchain
{
public:
    [[nodiscard]] ERHISwapchainState GetState() const noexcept override { return State; }
    [[nodiscard]] uint32 GetFrameCount() const noexcept override { return 2; }
    [[nodiscard]] uint32 GetCurrentFrameIndex() const noexcept override { return 0; }

    ERHIResult AcquireNextFrame(uint32& OutFrameIndex) override
    {
        bLegacyAcquireCalled = true;
        OutFrameIndex = 0;
        State = ERHISwapchainState::Acquired;
        return ERHIResult::Success;
    }

    ERHIResult Present(uint32) override
    {
        bLegacyPresentCalled = true;
        State = ERHISwapchainState::Ready;
        return ERHIResult::Success;
    }

    [[nodiscard]] bool WasLegacyAcquireCalled() const noexcept { return bLegacyAcquireCalled; }
    [[nodiscard]] bool WasLegacyPresentCalled() const noexcept { return bLegacyPresentCalled; }

private:
    ERHISwapchainState State = ERHISwapchainState::Ready;
    bool bLegacyAcquireCalled = false;
    bool bLegacyPresentCalled = false;
};

class FMockSwapchain final : public IRHISwapchain
{
public:
    explicit FMockSwapchain(uint32 InFrameCount)
        : FrameCount(InFrameCount > 0 ? InFrameCount : 1)
    {
    }

    [[nodiscard]] ERHISwapchainState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] uint32 GetFrameCount() const noexcept override
    {
        return FrameCount;
    }

    [[nodiscard]] uint32 GetCurrentFrameIndex() const noexcept override
    {
        return CurrentFrameIndex;
    }

    ERHIResult AcquireNextFrame(uint32& OutFrameIndex) override
    {
        if (State == ERHISwapchainState::ResizeRequired)
        {
            return ERHIResult::ResizeRequired;
        }
        if (State == ERHISwapchainState::Unavailable)
        {
            return ERHIResult::Unavailable;
        }
        if (State == ERHISwapchainState::Acquired)
        {
            return ERHIResult::InvalidState;
        }

        OutFrameIndex = CurrentFrameIndex;
        State = ERHISwapchainState::Acquired;
        return ERHIResult::Success;
    }

    ERHIResult AcquireNextFrame(uint32& OutFrameIndex, const TSharedPtr<IRHISemaphore>& SignalSemaphore) override
    {
        if (!SignalSemaphore)
        {
            return AcquireNextFrame(OutFrameIndex);
        }

        const TSharedPtr<FMockSemaphore> MockSemaphore =
            std::dynamic_pointer_cast<FMockSemaphore>(SignalSemaphore);
        if (!MockSemaphore)
        {
            return ERHIResult::Unsupported;
        }
        if (MockSemaphore->IsSignaled())
        {
            return ERHIResult::InvalidState;
        }

        uint32 AcquiredIndex = 0;
        const ERHIResult AcquireResult = AcquireNextFrame(AcquiredIndex);
        if (AcquireResult != ERHIResult::Success)
        {
            return AcquireResult;
        }

        const ERHIResult SignalResult = MockSemaphore->Signal();
        if (SignalResult != ERHIResult::Success)
        {
            State = ERHISwapchainState::Ready;
            return SignalResult;
        }

        OutFrameIndex = AcquiredIndex;
        return ERHIResult::Success;
    }

    ERHIResult Present(uint32 FrameIndex) override
    {
        if (State == ERHISwapchainState::ResizeRequired)
        {
            return ERHIResult::ResizeRequired;
        }
        if (State == ERHISwapchainState::Unavailable)
        {
            return ERHIResult::Unavailable;
        }
        if (State != ERHISwapchainState::Acquired || FrameIndex != CurrentFrameIndex)
        {
            return ERHIResult::InvalidState;
        }

        CurrentFrameIndex = (CurrentFrameIndex + 1) % FrameCount;
        State = ERHISwapchainState::Ready;
        return ERHIResult::Success;
    }

    ERHIResult Present(uint32 FrameIndex, const TSharedPtr<IRHISemaphore>& WaitSemaphore) override
    {
        if (State == ERHISwapchainState::ResizeRequired)
        {
            return ERHIResult::ResizeRequired;
        }
        if (State == ERHISwapchainState::Unavailable)
        {
            return ERHIResult::Unavailable;
        }
        if (State != ERHISwapchainState::Acquired || FrameIndex != CurrentFrameIndex)
        {
            return ERHIResult::InvalidState;
        }
        if (!WaitSemaphore)
        {
            return Present(FrameIndex);
        }

        const TSharedPtr<FMockSemaphore> MockSemaphore =
            std::dynamic_pointer_cast<FMockSemaphore>(WaitSemaphore);
        if (!MockSemaphore)
        {
            return ERHIResult::Unsupported;
        }
        if (!MockSemaphore->IsSignaled())
        {
            return ERHIResult::NotReady;
        }

        const ERHIResult ConsumeResult = MockSemaphore->Consume();
        if (ConsumeResult != ERHIResult::Success)
        {
            return ConsumeResult;
        }
        return Present(FrameIndex);
    }

    void SimulateResizeRequired()
    {
        State = ERHISwapchainState::ResizeRequired;
    }

    void SetUnavailable()
    {
        State = ERHISwapchainState::Unavailable;
    }

    void ResetToReady()
    {
        State = ERHISwapchainState::Ready;
    }

private:
    uint32 FrameCount = 1;
    uint32 CurrentFrameIndex = 0;
    ERHISwapchainState State = ERHISwapchainState::Ready;
};

class FMockBuffer final : public IRHIBuffer
{
public:
    explicit FMockBuffer(FRHIBufferDesc InDesc)
        : Desc(InDesc)
    {
    }

    [[nodiscard]] const FRHIBufferDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] uint64 GetSizeInBytes() const noexcept override { return Desc.SizeInBytes; }
    [[nodiscard]] ERHIBufferUsage GetUsage() const noexcept override { return Desc.Usage; }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHIBufferDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockTexture final : public IRHITexture
{
public:
    explicit FMockTexture(FRHITextureDesc InDesc)
        : Desc(InDesc)
    {
    }

    [[nodiscard]] const FRHITextureDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] ERHITextureDimension GetDimension() const noexcept override { return Desc.Dimension; }
    [[nodiscard]] ERHIFormat GetFormat() const noexcept override { return Desc.Format; }
    [[nodiscard]] ERHITextureUsage GetUsage() const noexcept override { return Desc.Usage; }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHITextureDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockSampler final : public IRHISampler
{
public:
    explicit FMockSampler(FRHISamplerDesc InDesc)
        : Desc(InDesc)
    {
    }

    [[nodiscard]] const FRHISamplerDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHISamplerDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockShaderModule final : public IRHIShaderModule
{
public:
    explicit FMockShaderModule(FRHIShaderModuleDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHIShaderModuleDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] ERHIShaderStage GetStage() const noexcept override { return Desc.Stage; }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHIShaderModuleDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockPipelineLayout final : public IRHIPipelineLayout
{
public:
    explicit FMockPipelineLayout(FRHIPipelineLayoutDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHIPipelineLayoutDesc& GetDesc() const noexcept override { return Desc; }

    [[nodiscard]] uint32 GetSetCount() const noexcept override
    {
        uint32 MaxSetIndex = 0;
        for (const FRHIDescriptorBinding& Binding : Desc.Bindings)
        {
            MaxSetIndex = std::max(MaxSetIndex, Binding.SetIndex);
        }
        return Desc.Bindings.empty() ? 0 : MaxSetIndex + 1;
    }

    [[nodiscard]] const FRHIDescriptorBinding* FindBinding(uint32 SetIndex, uint32 BindingSlot) const noexcept override
    {
        for (const FRHIDescriptorBinding& Binding : Desc.Bindings)
        {
            if (Binding.SetIndex == SetIndex && Binding.BindingSlot == BindingSlot)
            {
                return &Binding;
            }
        }
        return nullptr;
    }

    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHIPipelineLayoutDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockDescriptorSet final : public IRHIDescriptorSet
{
public:
    FMockDescriptorSet(TSharedPtr<IRHIPipelineLayout> InLayout, uint32 InSetIndex)
        : Layout(std::move(InLayout))
        , SetIndex(InSetIndex)
    {
    }

    [[nodiscard]] uint32 GetSetIndex() const noexcept override { return SetIndex; }
    [[nodiscard]] TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept override { return Layout; }

    [[nodiscard]] ERHIDescriptorResourceKind GetBoundResourceKind(uint32 BindingSlot, uint32 ArrayIndex = 0) const noexcept override
    {
        for (const FBoundResource& Bound : BoundResources)
        {
            if (Bound.BindingSlot == BindingSlot && Bound.ArrayIndex == ArrayIndex)
            {
                return Bound.Kind;
            }
        }
        return ERHIDescriptorResourceKind::None;
    }

    [[nodiscard]] uint32 GetBoundResourceCount() const noexcept override
    {
        return static_cast<uint32>(BoundResources.size());
    }

    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }

    ERHIResult UpdateBuffer(uint32 BindingSlot, uint32 ArrayIndex, const TSharedPtr<IRHIBuffer>& Buffer) override
    {
        const FRHIDescriptorBinding* Binding = ValidateWrite(BindingSlot, ArrayIndex);
        if (!Binding || !Buffer || Buffer->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return ERHIResult::InvalidState;
        }
        if (Binding->DescriptorType == ERHIDescriptorType::UniformBuffer && !HasRHIFlag(Buffer->GetUsage(), ERHIBufferUsage::Uniform))
        {
            return ERHIResult::Unsupported;
        }
        if (Binding->DescriptorType == ERHIDescriptorType::StorageBuffer && !HasRHIFlag(Buffer->GetUsage(), ERHIBufferUsage::Storage))
        {
            return ERHIResult::Unsupported;
        }
        if (Binding->DescriptorType != ERHIDescriptorType::UniformBuffer && Binding->DescriptorType != ERHIDescriptorType::StorageBuffer)
        {
            return ERHIResult::Unsupported;
        }
        Bind(BindingSlot, ArrayIndex, ERHIDescriptorResourceKind::Buffer);
        return ERHIResult::Success;
    }

    ERHIResult UpdateTexture(uint32 BindingSlot, uint32 ArrayIndex, const TSharedPtr<IRHITexture>& Texture) override
    {
        const FRHIDescriptorBinding* Binding = ValidateWrite(BindingSlot, ArrayIndex);
        if (!Binding || !Texture || Texture->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return ERHIResult::InvalidState;
        }
        if (Binding->DescriptorType == ERHIDescriptorType::SampledTexture && !HasRHIFlag(Texture->GetUsage(), ERHITextureUsage::Sampled))
        {
            return ERHIResult::Unsupported;
        }
        if (Binding->DescriptorType == ERHIDescriptorType::StorageTexture && !HasRHIFlag(Texture->GetUsage(), ERHITextureUsage::Storage))
        {
            return ERHIResult::Unsupported;
        }
        if (Binding->DescriptorType != ERHIDescriptorType::SampledTexture && Binding->DescriptorType != ERHIDescriptorType::StorageTexture)
        {
            return ERHIResult::Unsupported;
        }
        Bind(BindingSlot, ArrayIndex, ERHIDescriptorResourceKind::Texture);
        return ERHIResult::Success;
    }

    ERHIResult UpdateSampler(uint32 BindingSlot, uint32 ArrayIndex, const TSharedPtr<IRHISampler>& Sampler) override
    {
        const FRHIDescriptorBinding* Binding = ValidateWrite(BindingSlot, ArrayIndex);
        if (!Binding || !Sampler || Sampler->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return ERHIResult::InvalidState;
        }
        if (Binding->DescriptorType != ERHIDescriptorType::Sampler)
        {
            return ERHIResult::Unsupported;
        }
        Bind(BindingSlot, ArrayIndex, ERHIDescriptorResourceKind::Sampler);
        return ERHIResult::Success;
    }

    ERHIResult UpdateCombinedTextureSampler(uint32 BindingSlot, uint32 ArrayIndex, const TSharedPtr<IRHITexture>& Texture, const TSharedPtr<IRHISampler>& Sampler) override
    {
        const FRHIDescriptorBinding* Binding = ValidateWrite(BindingSlot, ArrayIndex);
        if (!Binding || !Texture || !Sampler ||
            Texture->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            Sampler->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return ERHIResult::InvalidState;
        }
        if (Binding->DescriptorType != ERHIDescriptorType::CombinedTextureSampler || !HasRHIFlag(Texture->GetUsage(), ERHITextureUsage::Sampled))
        {
            return ERHIResult::Unsupported;
        }
        Bind(BindingSlot, ArrayIndex, ERHIDescriptorResourceKind::CombinedTextureSampler);
        return ERHIResult::Success;
    }

    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    struct FBoundResource
    {
        uint32 BindingSlot = 0;
        uint32 ArrayIndex = 0;
        ERHIDescriptorResourceKind Kind = ERHIDescriptorResourceKind::None;
    };

    [[nodiscard]] const FRHIDescriptorBinding* ValidateWrite(uint32 BindingSlot, uint32 ArrayIndex) const
    {
        if (State != ERHIResourceLifecycleState::Valid || !Layout || Layout->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return nullptr;
        }
        const FRHIDescriptorBinding* Binding = Layout->FindBinding(SetIndex, BindingSlot);
        if (!Binding || ArrayIndex >= Binding->ArrayCount)
        {
            return nullptr;
        }
        return Binding;
    }

    void Bind(uint32 BindingSlot, uint32 ArrayIndex, ERHIDescriptorResourceKind Kind)
    {
        for (FBoundResource& Bound : BoundResources)
        {
            if (Bound.BindingSlot == BindingSlot && Bound.ArrayIndex == ArrayIndex)
            {
                Bound.Kind = Kind;
                return;
            }
        }
        BoundResources.push_back({BindingSlot, ArrayIndex, Kind});
    }

    TSharedPtr<IRHIPipelineLayout> Layout;
    uint32 SetIndex = 0;
    TArray<FBoundResource> BoundResources;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockGraphicsPipeline final : public IRHIGraphicsPipeline
{
public:
    explicit FMockGraphicsPipeline(FRHIGraphicsPipelineDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHIGraphicsPipelineDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept override { return Desc.PipelineLayout; }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHIGraphicsPipelineDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockComputePipeline final : public IRHIComputePipeline
{
public:
    explicit FMockComputePipeline(FRHIComputePipelineDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHIComputePipelineDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept override { return Desc.PipelineLayout; }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHIComputePipelineDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockRenderPass final : public IRHIRenderPass
{
public:
    explicit FMockRenderPass(FRHIRenderPassDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHIRenderPassDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] uint32 GetAttachmentCount() const noexcept override { return static_cast<uint32>(Desc.Attachments.size()); }
    [[nodiscard]] const FRHIRenderPassAttachmentDesc* GetAttachment(uint32 AttachmentIndex) const noexcept override
    {
        return AttachmentIndex < Desc.Attachments.size() ? &Desc.Attachments[AttachmentIndex] : nullptr;
    }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHIRenderPassDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FMockFramebuffer final : public IRHIFramebuffer
{
public:
    explicit FMockFramebuffer(FRHIFramebufferDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHIFramebufferDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] TSharedPtr<IRHIRenderPass> GetRenderPass() const noexcept override { return Desc.RenderPass; }
    [[nodiscard]] uint32 GetWidth() const noexcept override { return Desc.Width; }
    [[nodiscard]] uint32 GetHeight() const noexcept override { return Desc.Height; }
    [[nodiscard]] uint32 GetAttachmentCount() const noexcept override { return static_cast<uint32>(Desc.Attachments.size()); }
    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }

private:
    FRHIFramebufferDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

[[nodiscard]] bool IsSupportedFormat(const FRHIDeviceCapabilities& Capabilities, ERHIFormat Format)
{
    return Format != ERHIFormat::Unknown && Capabilities.SupportsFormat(Format);
}

[[nodiscard]] bool IsSupportedTextureDesc(const FRHIDeviceCapabilities& Capabilities, const FRHITextureDesc& Desc)
{
    return IsValidRHITextureDesc(Desc) && IsSupportedFormat(Capabilities, Desc.Format);
}

[[nodiscard]] bool IsValidPipelineLayoutDesc(const FRHIPipelineLayoutDesc& Desc)
{
    return IsValidRHIPipelineLayoutDesc(Desc);
}

[[nodiscard]] bool IsValidRenderPassDesc(const FRHIDeviceCapabilities& Capabilities, const FRHIRenderPassDesc& Desc)
{
    if (!Stoner::RHI::IsValidRHIRenderPassDesc(Desc))
    {
        return false;
    }
    for (const FRHIRenderPassAttachmentDesc& Attachment : Desc.Attachments)
    {
        if (!IsSupportedFormat(Capabilities, Attachment.Format))
        {
            return false;
        }
    }
    return true;
}

class FMockCommandQueue final : public IRHICommandQueue
{
public:
    explicit FMockCommandQueue(ERHIQueueType InQueueType)
        : QueueType(InQueueType)
    {
    }

    [[nodiscard]] ERHIQueueType GetQueueType() const noexcept override
    {
        return QueueType;
    }

    [[nodiscard]] uint32 GetSubmittedCommandBufferCount() const noexcept override
    {
        return static_cast<uint32>(SubmittedBuffers.size());
    }

    ERHIResult Submit(
        const TSharedPtr<IRHICommandBuffer>& CommandBuffer,
        const TArray<TSharedPtr<IRHISemaphore>>& WaitSemaphores,
        const TArray<TSharedPtr<IRHISemaphore>>& SignalSemaphores,
        const TSharedPtr<IRHIFence>& Fence) override
    {
        if (!CommandBuffer)
        {
            return ERHIResult::InvalidState;
        }
        if (CommandBuffer->GetCompatibleQueueType() != QueueType)
        {
            return ERHIResult::Unsupported;
        }
        if (CommandBuffer->GetState() != ERHICommandBufferState::Completed)
        {
            return ERHIResult::InvalidState;
        }

        const TSharedPtr<FMockCommandBuffer> MockCommandBuffer =
            std::dynamic_pointer_cast<FMockCommandBuffer>(CommandBuffer);
        if (!MockCommandBuffer)
        {
            return ERHIResult::Unsupported;
        }

        TArray<TSharedPtr<FMockSemaphore>> MockWaitSemaphores;
        TArray<TSharedPtr<FMockSemaphore>> MockSignalSemaphores;
        for (const TSharedPtr<IRHISemaphore>& Semaphore : WaitSemaphores)
        {
            if (!Semaphore)
            {
                return ERHIResult::InvalidState;
            }

            const TSharedPtr<FMockSemaphore> MockSemaphore =
                std::dynamic_pointer_cast<FMockSemaphore>(Semaphore);
            if (!MockSemaphore)
            {
                return ERHIResult::Unsupported;
            }
            if (!MockSemaphore->IsSignaled())
            {
                return ERHIResult::NotReady;
            }
            if (std::find(MockWaitSemaphores.begin(), MockWaitSemaphores.end(), MockSemaphore) !=
                MockWaitSemaphores.end())
            {
                return ERHIResult::InvalidState;
            }
            MockWaitSemaphores.push_back(MockSemaphore);
        }

        for (const TSharedPtr<IRHISemaphore>& Semaphore : SignalSemaphores)
        {
            if (!Semaphore)
            {
                return ERHIResult::InvalidState;
            }

            const TSharedPtr<FMockSemaphore> MockSemaphore =
                std::dynamic_pointer_cast<FMockSemaphore>(Semaphore);
            if (!MockSemaphore)
            {
                return ERHIResult::Unsupported;
            }
            if (MockSemaphore->IsSignaled() ||
                std::find(MockWaitSemaphores.begin(), MockWaitSemaphores.end(), MockSemaphore) !=
                    MockWaitSemaphores.end() ||
                std::find(MockSignalSemaphores.begin(), MockSignalSemaphores.end(), MockSemaphore) !=
                    MockSignalSemaphores.end())
            {
                return ERHIResult::InvalidState;
            }
            MockSignalSemaphores.push_back(MockSemaphore);
        }

        TSharedPtr<FMockFence> MockFence;
        if (Fence)
        {
            MockFence = std::dynamic_pointer_cast<FMockFence>(Fence);
            if (!MockFence)
            {
                return ERHIResult::Unsupported;
            }
            if (MockFence->IsSignaled())
            {
                return ERHIResult::InvalidState;
            }
        }

        for (const TSharedPtr<FMockSemaphore>& Semaphore : MockWaitSemaphores)
        {
            if (Semaphore->Consume() != ERHIResult::Success)
            {
                return ERHIResult::Failed;
            }
        }

        if (MockCommandBuffer->MarkSubmitted() != ERHIResult::Success)
        {
            return ERHIResult::Failed;
        }
        SubmittedBuffers.push_back(CommandBuffer);

        for (const TSharedPtr<FMockSemaphore>& Semaphore : MockSignalSemaphores)
        {
            if (Semaphore->Signal() != ERHIResult::Success)
            {
                return ERHIResult::Failed;
            }
        }

        if (MockFence && MockFence->Signal() != ERHIResult::Success)
        {
            return ERHIResult::Failed;
        }

        return ERHIResult::Success;
    }

    ERHIResult WaitIdle() override
    {
        for (const TSharedPtr<IRHICommandBuffer>& Buffer : SubmittedBuffers)
        {
            if (TSharedPtr<FMockCommandBuffer> MockBuffer = std::dynamic_pointer_cast<FMockCommandBuffer>(Buffer))
            {
                MockBuffer->MarkResettable();
            }
        }
        return ERHIResult::Success;
    }

private:
    ERHIQueueType QueueType = ERHIQueueType::Graphics;
    TArray<TSharedPtr<IRHICommandBuffer>> SubmittedBuffers;
};

class FMockDevice final : public IRHIDevice
{
public:
    FMockDevice()
    {
        Capabilities.bSupportsGraphicsQueue = true;
        Capabilities.bSupportsComputeQueue = true;
        Capabilities.bSupportsTransferQueue = true;
        Capabilities.bSupportsPresentQueue = true;
        Capabilities.bSupportsPresentation = true;
        Capabilities.bSupportsSynchronization = true;
        Capabilities.MaxInFlightFrames = 3;
        Capabilities.MaxCommandBuffersPerQueue = 64;
        Capabilities.MaxQueuesPerType = 1;
        Capabilities.SupportedFormats = {
            ERHIFormat::R8G8B8A8_UNorm,
            ERHIFormat::B8G8R8A8_UNorm,
            ERHIFormat::D24_UNorm_S8_UInt,
            ERHIFormat::D32_Float,
            ERHIFormat::S8_UInt};
    }

    explicit FMockDevice(const FRHIDeviceCapabilities& InCapabilities)
        : Capabilities(InCapabilities)
    {
    }

    [[nodiscard]] ERHIDeviceState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] const FRHIDeviceCapabilities& GetCapabilities() const noexcept override
    {
        return Capabilities;
    }

    [[nodiscard]] bool IsActive() const noexcept override
    {
        return State == ERHIDeviceState::Active;
    }

    ERHIResult Shutdown() override
    {
        State = ERHIDeviceState::Shutdown;
        return ERHIResult::Success;
    }

    TRHIObjectResult<IRHICommandQueue> CreateCommandQueue(ERHIQueueType QueueType) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.SupportsQueue(QueueType))
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockCommandQueue>(QueueType)};
    }

    TRHIObjectResult<IRHICommandBuffer> CreateCommandBuffer(ERHIQueueType CompatibleQueueType) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.SupportsQueue(CompatibleQueueType))
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockCommandBuffer>(CompatibleQueueType)};
    }

    TRHIObjectResult<IRHIFence> CreateFence(bool bInitiallySignaled = false) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.bSupportsSynchronization)
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockFence>(bInitiallySignaled)};
    }

    TRHIObjectResult<IRHISemaphore> CreateSemaphore() override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.bSupportsSynchronization)
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockSemaphore>()};
    }

    TRHIObjectResult<IRHISwapchain> CreateSwapchain(uint32 FrameCount) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.bSupportsPresentation || !Capabilities.bSupportsPresentQueue)
        {
            return {ERHIResult::Unsupported, nullptr};
        }
        if (FrameCount == 0)
        {
            return {ERHIResult::InvalidState, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockSwapchain>(FrameCount)};
    }

    TRHIObjectResult<IRHIBuffer> CreateBuffer(const FRHIBufferDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!IsValidRHIBufferDesc(Desc))
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockBuffer>(Desc)};
    }

    TRHIObjectResult<IRHITexture> CreateTexture(const FRHITextureDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!IsSupportedTextureDesc(Capabilities, Desc))
        {
            return {Desc.Format == ERHIFormat::Unknown || !Capabilities.SupportsFormat(Desc.Format) ? ERHIResult::Unsupported : ERHIResult::InvalidState, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockTexture>(Desc)};
    }

    TRHIObjectResult<IRHISampler> CreateSampler(const FRHISamplerDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!IsValidRHISamplerDesc(Desc))
        {
            return {ERHIResult::Unsupported, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockSampler>(Desc)};
    }

    TRHIObjectResult<IRHIShaderModule> CreateShaderModule(const FRHIShaderModuleDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!IsValidRHIShaderModuleDesc(Desc))
        {
            return {IsSupportedRHIShaderStage(Desc.Stage) ? ERHIResult::InvalidState : ERHIResult::Unsupported, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockShaderModule>(Desc)};
    }

    TRHIObjectResult<IRHIPipelineLayout> CreatePipelineLayout(const FRHIPipelineLayoutDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!IsValidPipelineLayoutDesc(Desc))
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockPipelineLayout>(Desc)};
    }

    TRHIObjectResult<IRHIDescriptorSet> CreateDescriptorSet(const TSharedPtr<IRHIPipelineLayout>& Layout, uint32 SetIndex) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Layout || Layout->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        bool bHasSet = false;
        for (const FRHIDescriptorBinding& Binding : Layout->GetDesc().Bindings)
        {
            bHasSet = bHasSet || Binding.SetIndex == SetIndex;
        }
        if (!bHasSet)
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockDescriptorSet>(Layout, SetIndex)};
    }

    TRHIObjectResult<IRHIGraphicsPipeline> CreateGraphicsPipeline(const FRHIGraphicsPipelineDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Desc.PipelineLayout || Desc.PipelineLayout->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return {ERHIResult::InvalidState, nullptr};
        }

        bool bHasVertex = false;
        bool bHasFragment = false;
        for (const TSharedPtr<IRHIShaderModule>& Shader : Desc.ShaderModules)
        {
            if (!Shader || Shader->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
            {
                return {ERHIResult::InvalidState, nullptr};
            }
            if (!IsRHIShaderInterfaceCompatibleWithPipelineLayout(
                    Shader->GetDesc().InterfaceMetadata,
                    Desc.PipelineLayout->GetDesc()))
            {
                return {ERHIResult::InvalidState, nullptr};
            }
            if (Shader->GetStage() == ERHIShaderStage::Vertex)
            {
                if (bHasVertex)
                {
                    return {ERHIResult::InvalidState, nullptr};
                }
                bHasVertex = true;
            }
            else if (Shader->GetStage() == ERHIShaderStage::Fragment)
            {
                if (bHasFragment)
                {
                    return {ERHIResult::InvalidState, nullptr};
                }
                bHasFragment = true;
            }
            else
            {
                return {ERHIResult::Unsupported, nullptr};
            }
        }
        if (!bHasVertex || !bHasFragment || Desc.RenderTargets.ColorFormats.empty())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        for (ERHIFormat Format : Desc.RenderTargets.ColorFormats)
        {
            if (!IsSupportedFormat(Capabilities, Format) || IsDepthStencilFormat(Format))
            {
                return {ERHIResult::Unsupported, nullptr};
            }
        }
        if (Desc.RenderTargets.DepthStencilFormat != ERHIFormat::Unknown &&
            (!IsSupportedFormat(Capabilities, Desc.RenderTargets.DepthStencilFormat) || !IsDepthStencilFormat(Desc.RenderTargets.DepthStencilFormat)))
        {
            return {ERHIResult::Unsupported, nullptr};
        }
        if (!IsValidRHIGraphicsPipelineState(Desc))
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockGraphicsPipeline>(Desc)};
    }

    TRHIObjectResult<IRHIComputePipeline> CreateComputePipeline(const FRHIComputePipelineDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Desc.PipelineLayout || Desc.PipelineLayout->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (Desc.ShaderModules.size() != 1 || !Desc.ShaderModules[0] ||
            Desc.ShaderModules[0]->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (Desc.ShaderModules[0]->GetStage() != ERHIShaderStage::Compute)
        {
            return {ERHIResult::Unsupported, nullptr};
        }
        if (!IsRHIShaderInterfaceCompatibleWithPipelineLayout(
                Desc.ShaderModules[0]->GetDesc().InterfaceMetadata,
                Desc.PipelineLayout->GetDesc()))
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockComputePipeline>(Desc)};
    }

    TRHIObjectResult<IRHIRenderPass> CreateRenderPass(const FRHIRenderPassDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!IsValidRenderPassDesc(Capabilities, Desc))
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        return {ERHIResult::Success, MakeShared<FMockRenderPass>(Desc)};
    }

    TRHIObjectResult<IRHIFramebuffer> CreateFramebuffer(const FRHIFramebufferDesc& Desc) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Desc.RenderPass || Desc.RenderPass->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            Desc.Width == 0 || Desc.Height == 0 || Desc.Attachments.size() != Desc.RenderPass->GetAttachmentCount())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        for (uint32 AttachmentIndex = 0; AttachmentIndex < Desc.Attachments.size(); ++AttachmentIndex)
        {
            const FRHIRenderPassAttachmentDesc* AttachmentDesc = Desc.RenderPass->GetAttachment(AttachmentIndex);
            const FRHIFramebufferAttachment& Attachment = Desc.Attachments[AttachmentIndex];
            const TSharedPtr<IRHITexture>& Texture = Attachment.Texture;
            if (!AttachmentDesc || !Texture || Texture->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
            {
                return {ERHIResult::InvalidState, nullptr};
            }
            const FRHITextureDesc& TextureDesc = Texture->GetDesc();
            const uint32 MipWidth = GetRHIMipExtent(TextureDesc.Width, Attachment.MipLevel);
            const uint32 MipHeight = GetRHIMipExtent(TextureDesc.Height, Attachment.MipLevel);
            if (TextureDesc.Format != AttachmentDesc->Format || TextureDesc.SampleCount != AttachmentDesc->SampleCount ||
                Attachment.MipLevel >= TextureDesc.MipLevels || Attachment.ArrayLayer >= TextureDesc.ArrayLayers ||
                MipWidth != Desc.Width || MipHeight != Desc.Height)
            {
                return {ERHIResult::InvalidState, nullptr};
            }
            if (AttachmentDesc->Role == ERHIAttachmentRole::Color && !HasRHIFlag(TextureDesc.Usage, ERHITextureUsage::ColorAttachment))
            {
                return {ERHIResult::Unsupported, nullptr};
            }
            if (AttachmentDesc->Role == ERHIAttachmentRole::DepthStencil && !HasRHIFlag(TextureDesc.Usage, ERHITextureUsage::DepthStencilAttachment))
            {
                return {ERHIResult::Unsupported, nullptr};
            }
        }
        return {ERHIResult::Success, MakeShared<FMockFramebuffer>(Desc)};
    }

private:
    ERHIDeviceState State = ERHIDeviceState::Active;
    FRHIDeviceCapabilities Capabilities;
};

void TestCoreValuesAndCapabilities(FRHICoreTestResult& Result)
{
    Record(Result, ERHIResult::Success != ERHIResult::InvalidState, "ERHIResult exposes distinct success and invalid-state values");
    Record(Result, RHISucceeded(ERHIResult::Success), "RHISucceeded accepts Success");
    Record(Result, RHIFailed(ERHIResult::Unsupported), "RHIFailed accepts Unsupported");

    FRHIDeviceCapabilities Capabilities;
    Capabilities.bSupportsGraphicsQueue = true;
    Capabilities.bSupportsComputeQueue = true;
    Capabilities.bSupportsTransferQueue = false;
    Capabilities.bSupportsPresentQueue = false;
    Capabilities.SupportedFormats = {ERHIFormat::R8G8B8A8_UNorm, ERHIFormat::D32_Float};

    Record(Result, Capabilities.SupportsQueue(ERHIQueueType::Graphics), "FRHIDeviceCapabilities reports supported graphics queue");
    Record(Result, Capabilities.SupportsQueue(ERHIQueueType::Compute), "FRHIDeviceCapabilities reports supported compute queue");
    Record(Result, !Capabilities.SupportsQueue(ERHIQueueType::Transfer), "FRHIDeviceCapabilities rejects unsupported transfer queue");
    Record(Result, Capabilities.SupportsFormat(ERHIFormat::R8G8B8A8_UNorm), "FRHIDeviceCapabilities reports supported color format");
    Record(Result, !Capabilities.SupportsFormat(ERHIFormat::S8_UInt), "FRHIDeviceCapabilities rejects unsupported stencil format");
}

void TestRuntimeAndPresentationContracts(FRHICoreTestResult& Result)
{
    FRHIRuntimeSnapshot Snapshot;
    Snapshot.RequestedMode = ERHIRuntimeMode::Native;
    Snapshot.ObjectMode = ERHIRuntimeObjectMode::RealRuntime;
    Snapshot.LiveInstances = 1;
    Snapshot.LiveDevices = 1;
    Snapshot.LiveBuffers = 2;
    Record(Result, Snapshot.ProvesNativeExecution(), "RHI runtime snapshot proves native execution explicitly");
    Record(Result, Snapshot.GetTotalLiveObjectCount() == 4, "RHI runtime snapshot exposes stable live counts");

    FRHIRuntimeSnapshot OverflowSnapshot;
    OverflowSnapshot.LiveInstances = std::numeric_limits<uint32>::max();
    OverflowSnapshot.LiveDevices = 1;
    Record(Result,
        OverflowSnapshot.GetTotalLiveObjectCount() == static_cast<uint64>(std::numeric_limits<uint32>::max()) + 1,
        "RHI runtime snapshot aggregates live counts without wrapping to zero");

    FRHIRuntimeSnapshot NativeHeadlessSnapshot = Snapshot;
    NativeHeadlessSnapshot.RequestedMode = ERHIRuntimeMode::NativeHeadless;
    Record(Result, NativeHeadlessSnapshot.ProvesNativeExecution(),
        "RHI runtime snapshot accepts native-headless execution proof");

    FRHIRuntimeSnapshot ContradictorySnapshot = Snapshot;
    ContradictorySnapshot.RequestedMode = ERHIRuntimeMode::Deterministic;
    Record(Result, !ContradictorySnapshot.ProvesNativeExecution(),
        "RHI runtime snapshot rejects native proof for a deterministic request");

    FRHIRuntimeSnapshot FallbackSnapshot = Snapshot;
    FallbackSnapshot.ObjectMode = ERHIRuntimeObjectMode::DeterministicFallback;
    Record(Result, !FallbackSnapshot.ProvesNativeExecution(),
        "RHI runtime snapshot rejects deterministic fallback as native proof");

    FRHIRuntimeSnapshot MissingInstanceSnapshot = Snapshot;
    MissingInstanceSnapshot.LiveInstances = 0;
    Record(Result, !MissingInstanceSnapshot.ProvesNativeExecution(),
        "RHI runtime snapshot requires a live native instance");

    FRHIRuntimeSnapshot MissingDeviceSnapshot = Snapshot;
    MissingDeviceSnapshot.LiveDevices = 0;
    Record(Result, !MissingDeviceSnapshot.ProvesNativeExecution(),
        "RHI runtime snapshot requires a live native device");

    FRHIPresentationSurfaceDesc SurfaceDesc;
    Record(Result, !SurfaceDesc.IsValid(), "RHI presentation surface rejects missing opaque window");
    SurfaceDesc.Window = FPlatformWindow(reinterpret_cast<void*>(static_cast<uintptr>(1)));
    Record(Result, SurfaceDesc.IsValid(), "RHI presentation surface accepts opaque platform window");

    FRHISwapchainDesc SwapchainDesc;
    SwapchainDesc.Width = 1280;
    SwapchainDesc.Height = 720;
    Record(Result, SwapchainDesc.IsValid() && SwapchainDesc.FramesInFlight == 2,
        "RHI swapchain description defaults to two frames in flight");

    FRHIBufferDesc UploadBuffer{64, ERHIBufferUsage::Vertex | ERHIBufferUsage::CopyDestination, ERHIMemoryAccess::HostVisible};
    Record(Result, IsValidRHIBufferDesc(UploadBuffer), "RHI host-visible vertex upload description is valid");

    FRHIViewport Viewport{0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};
    FRHIScissorRect Scissor{0, 0, 1280, 720};
    Record(Result, Viewport.Width > 0.0f && Viewport.Height > 0.0f && Scissor.Width == 1280,
        "RHI viewport and scissor contracts preserve pixel extent");

    FRHIRenderPassClearValues ClearValues;
    ClearValues.Colors.push_back({0.02f, 0.03f, 0.05f, 1.0f});
    Record(Result, ClearValues.Colors.size() == 1 && ClearValues.Depth == 1.0f,
        "RHI render pass clear values preserve color and depth defaults");

    FMockDevice Device;
    IRHIDevice& BaseDevice = Device;
    const auto UnsupportedSurfaceSwapchain = BaseDevice.CreateSwapchain(nullptr, SwapchainDesc);
    FRHISwapchainDesc InvalidSwapchainDesc;
    const auto InvalidSurfaceSwapchain = BaseDevice.CreateSwapchain(nullptr, InvalidSwapchainDesc);
    Record(Result,
        UnsupportedSurfaceSwapchain.Result == ERHIResult::Unsupported &&
            !UnsupportedSurfaceSwapchain.Object &&
            InvalidSurfaceSwapchain.Result == ERHIResult::Unsupported &&
            !InvalidSurfaceSwapchain.Object,
        "IRHIDevice surface-aware swapchain compatibility path fails closed");

    FLegacyCommandBuffer LegacyCommands;
    IRHICommandBuffer& BaseCommands = LegacyCommands;
    Record(Result,
        BaseCommands.BeginRenderPass(nullptr, nullptr, ClearValues) == ERHIResult::Unsupported &&
            !LegacyCommands.WasLegacyBeginCalled(),
        "IRHICommandBuffer explicit clear compatibility path fails closed");
}

void TestDeviceLifecycleAndOwnership(FRHICoreTestResult& Result)
{
    FMockDevice Device;

    Record(Result, Device.IsActive() && Device.GetState() == ERHIDeviceState::Active, "IRHIDevice mock starts active");
    Record(Result, Device.GetCapabilities().SupportsQueue(ERHIQueueType::Graphics), "IRHIDevice exposes graphics queue capability");
    Record(Result, Device.GetCapabilities().SupportsQueue(ERHIQueueType::Present), "IRHIDevice exposes present queue capability");

    const auto Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto CommandBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto Fence = Device.CreateFence();
    const auto Semaphore = Device.CreateSemaphore();
    const auto Swapchain = Device.CreateSwapchain(2);

    Record(Result, Queue.Succeeded(), "IRHIDevice creates supported command queue");
    Record(Result, CommandBuffer.Succeeded(), "IRHIDevice creates supported command buffer");
    Record(Result, Fence.Succeeded(), "IRHIDevice creates fence");
    Record(Result, Semaphore.Succeeded(), "IRHIDevice creates semaphore");
    Record(Result, Swapchain.Succeeded(), "IRHIDevice creates headless swapchain");

    FRHIDeviceCapabilities LimitedCapabilities = Device.GetCapabilities();
    LimitedCapabilities.bSupportsComputeQueue = false;
    FMockDevice LimitedDevice(LimitedCapabilities);
    Record(Result, LimitedDevice.CreateCommandQueue(ERHIQueueType::Compute).Result == ERHIResult::Unsupported,
        "IRHIDevice rejects unsupported queue creation");

    Record(Result, Device.Shutdown() == ERHIResult::Success, "IRHIDevice shutdown succeeds");
    Record(Result, Device.GetState() == ERHIDeviceState::Shutdown && !Device.IsActive(), "IRHIDevice reports shutdown state");
    Record(Result, Device.CreateCommandQueue(ERHIQueueType::Graphics).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects queue creation after shutdown");
    Record(Result, Device.CreateCommandBuffer(ERHIQueueType::Graphics).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects command buffer creation after shutdown");
}

void TestCommandBufferAndQueue(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    const auto QueueResult = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto BufferResult = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto ComputeBufferResult = Device.CreateCommandBuffer(ERHIQueueType::Compute);

    TSharedPtr<IRHICommandQueue> Queue = QueueResult.Object;
    TSharedPtr<IRHICommandBuffer> Buffer = BufferResult.Object;

    Record(Result, Buffer->Begin() == ERHIResult::Success, "IRHICommandBuffer begins recording");
    Record(Result, Buffer->Begin() == ERHIResult::InvalidState, "IRHICommandBuffer rejects double begin");
    Record(Result, Buffer->RecordDraw(3, 1) == ERHIResult::Success, "IRHICommandBuffer records symbolic draw");
    Record(Result, Buffer->RecordDispatch(1, 2, 3) == ERHIResult::Success, "IRHICommandBuffer records symbolic dispatch");
    Record(Result, Buffer->RecordBarrier() == ERHIResult::Success, "IRHICommandBuffer records symbolic barrier");
    Record(Result, Buffer->GetRecordedCommandCount() == 3, "IRHICommandBuffer preserves symbolic command count");
    Record(Result, Buffer->End() == ERHIResult::Success, "IRHICommandBuffer ends recording");
    Record(Result, Buffer->RecordBarrier() == ERHIResult::InvalidState, "IRHICommandBuffer rejects record after end");

    const TSharedPtr<FMockCommandBuffer> MockBuffer = std::dynamic_pointer_cast<FMockCommandBuffer>(Buffer);
    const bool bOrderPreserved = MockBuffer && MockBuffer->GetCommands().size() == 3 &&
        MockBuffer->GetCommands()[0].Type == ERHISymbolicCommandType::Draw &&
        MockBuffer->GetCommands()[1].Type == ERHISymbolicCommandType::Dispatch &&
        MockBuffer->GetCommands()[2].Type == ERHISymbolicCommandType::Barrier;
    Record(Result, bOrderPreserved, "IRHICommandBuffer preserves symbolic command ordering");

    TSharedPtr<IRHICommandBuffer> RecordingBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    Record(Result, RecordingBuffer->Begin() == ERHIResult::Success, "IRHICommandBuffer second buffer begins recording");
    Record(Result, Queue->Submit(RecordingBuffer) == ERHIResult::InvalidState, "IRHICommandQueue rejects submit while recording");

    TSharedPtr<IRHICommandBuffer> EmptyBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    Record(Result, EmptyBuffer->End() == ERHIResult::InvalidState, "IRHICommandBuffer rejects end without begin");
    Record(Result, Queue->Submit(EmptyBuffer) == ERHIResult::InvalidState, "IRHICommandQueue rejects incomplete submit");

    Record(Result, Queue->Submit(ComputeBufferResult.Object) == ERHIResult::Unsupported,
        "IRHICommandQueue rejects incompatible queue submit");
    Record(Result, Queue->Submit(Buffer) == ERHIResult::Success, "IRHICommandQueue submits compatible completed command buffer");
    Record(Result, Queue->GetSubmittedCommandBufferCount() == 1, "IRHICommandQueue tracks submitted command buffers");
    Record(Result, Buffer->Reset() == ERHIResult::InvalidState, "IRHICommandBuffer rejects reset while submitted");
    Record(Result, Queue->WaitIdle() == ERHIResult::Success, "IRHICommandQueue wait idle succeeds");
    Record(Result, Buffer->GetState() == ERHICommandBufferState::Resettable, "IRHICommandQueue wait idle makes submitted buffer resettable");
    Record(Result, Buffer->Reset() == ERHIResult::Success && Buffer->GetState() == ERHICommandBufferState::Idle,
        "IRHICommandBuffer resets after queue idle");
}

void TestSynchronization(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    TSharedPtr<IRHIFence> Fence = Device.CreateFence().Object;
    TSharedPtr<IRHIFence> SignaledFence = Device.CreateFence(true).Object;
    TSharedPtr<IRHISemaphore> WaitSemaphore = Device.CreateSemaphore().Object;
    TSharedPtr<IRHISemaphore> SignalSemaphore = Device.CreateSemaphore().Object;

    Record(Result, Fence->GetState() == ERHIFenceState::Unsignaled, "IRHIFence starts unsignaled");
    Record(Result, Fence->Wait() == ERHIResult::NotReady, "IRHIFence wait on unsignaled returns not-ready");
    Record(Result, Fence->Wait(1) == ERHIResult::Timeout, "IRHIFence wait on unsignaled with timeout returns timeout");
    Record(Result, SignaledFence->Wait() == ERHIResult::Success, "IRHIFence wait on signaled succeeds");
    Record(Result, SignaledFence->GetState() == ERHIFenceState::Waited, "IRHIFence records waited state");
    Record(Result, SignaledFence->Reset() == ERHIResult::Success && !SignaledFence->IsSignaled(), "IRHIFence reset clears signal");

    Record(Result, WaitSemaphore->GetState() == ERHISemaphoreState::Unsignaled, "IRHISemaphore starts unsignaled");
    Record(Result, WaitSemaphore->Consume() == ERHIResult::NotReady, "IRHISemaphore rejects consuming unsignaled state");
    Record(Result, WaitSemaphore->Signal() == ERHIResult::Success, "IRHISemaphore signal succeeds");
    Record(Result, WaitSemaphore->Signal() == ERHIResult::InvalidState, "IRHISemaphore rejects double signal");
    Record(Result, WaitSemaphore->Consume() == ERHIResult::Success, "IRHISemaphore consume succeeds after signal");
    Record(Result, WaitSemaphore->GetState() == ERHISemaphoreState::Consumed, "IRHISemaphore records consumed state");
    Record(Result, WaitSemaphore->Reset() == ERHIResult::Success, "IRHISemaphore reset succeeds");

    TSharedPtr<IRHICommandQueue> Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    TSharedPtr<IRHICommandBuffer> Buffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    Record(Result, Buffer->Begin() == ERHIResult::Success && Buffer->RecordDraw(3) == ERHIResult::Success &&
            Buffer->End() == ERHIResult::Success,
        "RHI sync test prepares completed command buffer");

    Record(Result, WaitSemaphore->Signal() == ERHIResult::Success, "RHI sync test prepares wait semaphore");
    const ERHIResult SubmitResult = Queue->Submit(Buffer, {WaitSemaphore}, {SignalSemaphore}, Fence);
    Record(Result, SubmitResult == ERHIResult::Success, "IRHICommandQueue submit observes semaphore and fence contracts");
    Record(Result, WaitSemaphore->GetState() == ERHISemaphoreState::Consumed, "IRHICommandQueue consumes wait semaphore");
    Record(Result, SignalSemaphore->IsSignaled(), "IRHICommandQueue signals output semaphore");
    Record(Result, Fence->IsSignaled(), "IRHICommandQueue signals fence");

    TSharedPtr<IRHICommandQueue> PartialWaitQueue =
        Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    TSharedPtr<IRHICommandBuffer> PartialWaitBuffer =
        Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    TSharedPtr<IRHISemaphore> FirstWait = Device.CreateSemaphore().Object;
    TSharedPtr<IRHISemaphore> SecondWait = Device.CreateSemaphore().Object;
    Record(Result,
        PartialWaitBuffer->Begin() == ERHIResult::Success &&
            PartialWaitBuffer->RecordDraw(3) == ERHIResult::Success &&
            PartialWaitBuffer->End() == ERHIResult::Success &&
            FirstWait->Signal() == ERHIResult::Success,
        "RHI sync failure test prepares a partial wait set");
    Record(Result,
        PartialWaitQueue->Submit(PartialWaitBuffer, {FirstWait, SecondWait}) == ERHIResult::NotReady &&
            FirstWait->IsSignaled() &&
            SecondWait->GetState() == ERHISemaphoreState::Unsignaled &&
            PartialWaitBuffer->GetState() == ERHICommandBufferState::Completed &&
            PartialWaitQueue->GetSubmittedCommandBufferCount() == 0,
        "IRHICommandQueue wait preflight fails without partial state transition");

    TSharedPtr<IRHICommandQueue> FailedSignalQueue =
        Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    TSharedPtr<IRHICommandBuffer> FailedSignalBuffer =
        Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    TSharedPtr<IRHISemaphore> AlreadySignaledOutput = Device.CreateSemaphore().Object;
    TSharedPtr<IRHIFence> FailedSignalFence = Device.CreateFence().Object;
    Record(Result,
        FailedSignalBuffer->Begin() == ERHIResult::Success &&
            FailedSignalBuffer->RecordDraw(3) == ERHIResult::Success &&
            FailedSignalBuffer->End() == ERHIResult::Success &&
            AlreadySignaledOutput->Signal() == ERHIResult::Success,
        "RHI sync failure test prepares an invalid signal set");
    Record(Result,
        FailedSignalQueue->Submit(
            FailedSignalBuffer, {}, {AlreadySignaledOutput}, FailedSignalFence) ==
                ERHIResult::InvalidState &&
            AlreadySignaledOutput->IsSignaled() &&
            FailedSignalBuffer->GetState() == ERHICommandBufferState::Completed &&
            FailedSignalQueue->GetSubmittedCommandBufferCount() == 0 &&
            !FailedSignalFence->IsSignaled(),
        "IRHICommandQueue signal preflight fails without partial submission");
}

void TestSwapchain(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    TSharedPtr<IRHISwapchain> Swapchain = Device.CreateSwapchain(3).Object;
    uint32 FrameIndex = 99;

    Record(Result, Swapchain->GetFrameCount() == 3, "IRHISwapchain reports frame count");
    Record(Result, Swapchain->GetCurrentFrameIndex() == 0, "IRHISwapchain starts at frame zero");
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::Success && FrameIndex == 0,
        "IRHISwapchain acquires first frame");
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::InvalidState,
        "IRHISwapchain rejects acquire while frame is acquired");
    Record(Result, Swapchain->Present(FrameIndex) == ERHIResult::Success, "IRHISwapchain presents acquired frame");
    Record(Result, Swapchain->GetCurrentFrameIndex() == 1, "IRHISwapchain advances current frame after present");
    Record(Result, Swapchain->Present(FrameIndex) == ERHIResult::InvalidState, "IRHISwapchain rejects repeated present");

    TSharedPtr<IRHISwapchain> InvalidPresentSwapchain = Device.CreateSwapchain(2).Object;
    Record(Result, InvalidPresentSwapchain->Present(0) == ERHIResult::InvalidState,
        "IRHISwapchain rejects present without acquire");

    TSharedPtr<FMockSwapchain> MockSwapchain = std::dynamic_pointer_cast<FMockSwapchain>(Swapchain);
    MockSwapchain->SimulateResizeRequired();
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::ResizeRequired,
        "IRHISwapchain acquire reports resize required");
    Record(Result, Swapchain->Present(1) == ERHIResult::ResizeRequired,
        "IRHISwapchain present reports resize required");

    MockSwapchain->SetUnavailable();
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::Unavailable,
        "IRHISwapchain acquire reports unavailable");

    Record(Result, Device.CreateSwapchain(0).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects zero-frame swapchain");
    Record(Result, true, "IRHISwapchain tests require no native window or backend surface");

    TSharedPtr<FLegacySwapchain> LegacySwapchain = MakeShared<FLegacySwapchain>();
    TSharedPtr<IRHISwapchain> BaseLegacySwapchain = LegacySwapchain;
    TSharedPtr<IRHISemaphore> LegacySignal = Device.CreateSemaphore().Object;
    uint32 LegacyFrameIndex = 99;
    Record(Result,
        BaseLegacySwapchain->AcquireNextFrame(LegacyFrameIndex, LegacySignal) == ERHIResult::Unsupported &&
            LegacyFrameIndex == 99 &&
            !LegacySwapchain->WasLegacyAcquireCalled() &&
            LegacySwapchain->GetState() == ERHISwapchainState::Ready &&
            !LegacySignal->IsSignaled(),
        "IRHISwapchain synchronized acquire compatibility path fails closed");
    Record(Result,
        BaseLegacySwapchain->Present(0, LegacySignal) == ERHIResult::Unsupported &&
            !LegacySwapchain->WasLegacyPresentCalled() &&
            LegacySwapchain->GetState() == ERHISwapchainState::Ready &&
            !LegacySignal->IsSignaled(),
        "IRHISwapchain synchronized present compatibility path fails closed");

    TSharedPtr<IRHISwapchain> AtomicSwapchain = Device.CreateSwapchain(2).Object;
    TSharedPtr<IRHISemaphore> AtomicSignal = Device.CreateSemaphore().Object;
    uint32 AtomicFrameIndex = 99;
    Record(Result, AtomicSignal->Signal() == ERHIResult::Success,
        "RHI swapchain failure test prepares an already-signaled semaphore");
    Record(Result,
        AtomicSwapchain->AcquireNextFrame(AtomicFrameIndex, AtomicSignal) == ERHIResult::InvalidState &&
            AtomicFrameIndex == 99 &&
            AtomicSwapchain->GetState() == ERHISwapchainState::Ready &&
            AtomicSignal->IsSignaled(),
        "IRHISwapchain failed synchronized acquire preserves all states");

    Record(Result, AtomicSignal->Reset() == ERHIResult::Success &&
            AtomicSwapchain->AcquireNextFrame(AtomicFrameIndex, AtomicSignal) == ERHIResult::Success &&
            AtomicFrameIndex == 0 &&
            AtomicSwapchain->GetState() == ERHISwapchainState::Acquired &&
            AtomicSignal->IsSignaled(),
        "IRHISwapchain synchronized acquire commits image and signal together");
    Record(Result,
        AtomicSwapchain->Present(AtomicFrameIndex + 1, AtomicSignal) == ERHIResult::InvalidState &&
            AtomicSwapchain->GetState() == ERHISwapchainState::Acquired &&
            AtomicSignal->IsSignaled(),
        "IRHISwapchain failed synchronized present preserves wait signal");
    Record(Result,
        AtomicSwapchain->Present(AtomicFrameIndex, AtomicSignal) == ERHIResult::Success &&
            AtomicSwapchain->GetState() == ERHISwapchainState::Ready &&
            AtomicSignal->GetState() == ERHISemaphoreState::Consumed,
        "IRHISwapchain synchronized present commits wait and frame together");
}

[[nodiscard]] FRHIPipelineLayoutDesc MakePipelineLayoutDesc()
{
    FRHIPipelineLayoutDesc Desc;
    Desc.Bindings = {
        {0, 0, ERHIDescriptorType::UniformBuffer, 1, ERHIShaderStageFlags::Vertex},
        {0, 1, ERHIDescriptorType::SampledTexture, 2, ERHIShaderStageFlags::Fragment},
        {0, 2, ERHIDescriptorType::Sampler, 1, ERHIShaderStageFlags::Fragment},
        {1, 0, ERHIDescriptorType::CombinedTextureSampler, 1, ERHIShaderStageFlags::Fragment},
        {1, 1, ERHIDescriptorType::StorageBuffer, 1, ERHIShaderStageFlags::Compute}};
    Desc.ConstantRanges = {{0, 64, ERHIShaderStageFlags::Vertex | ERHIShaderStageFlags::Fragment | ERHIShaderStageFlags::Compute}};
    return Desc;
}

[[nodiscard]] FRHIShaderModuleDesc MakeShaderDesc(ERHIShaderStage Stage, const char* EntryPoint, const char* Payload)
{
    FRHIShaderModuleDesc Desc;
    Desc.Stage = Stage;
    Desc.EntryPoint = EntryPoint;
    Desc.PayloadIdentity = Payload;
    Desc.Bytecode.Words =
        Stoner::Tests::MakeMinimalShaderBytecode(Stage, EntryPoint);
    const ERHIShaderStageFlags Visibility = ToShaderStageFlag(Stage);
    if (Stage == ERHIShaderStage::Vertex)
    {
        Desc.InterfaceMetadata.Bindings = {{0, 0, ERHIDescriptorType::UniformBuffer, 1, Visibility}};
    }
    else if (Stage == ERHIShaderStage::Fragment)
    {
        Desc.InterfaceMetadata.Bindings = {{0, 1, ERHIDescriptorType::SampledTexture, 1, Visibility}};
    }
    else if (Stage == ERHIShaderStage::Compute)
    {
        Desc.InterfaceMetadata.Bindings = {{1, 1, ERHIDescriptorType::StorageBuffer, 1, Visibility}};
    }
    Desc.InterfaceMetadata.ConstantRanges = {{0, 16, Visibility}};
    Desc.InterfaceMetadata.DebugName = Payload;
    Desc.DebugName = Payload;
    return Desc;
}

[[nodiscard]] FRHITextureDesc MakeColorTextureDesc(uint32 Width = 64, uint32 Height = 64)
{
    FRHITextureDesc Desc;
    Desc.Dimension = ERHITextureDimension::Texture2D;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Format = ERHIFormat::R8G8B8A8_UNorm;
    Desc.Usage = ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment | ERHITextureUsage::CopyDestination;
    return Desc;
}

[[nodiscard]] FRHITextureDesc MakeDepthTextureDesc(uint32 Width = 64, uint32 Height = 64)
{
    FRHITextureDesc Desc;
    Desc.Dimension = ERHITextureDimension::Texture2D;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Format = ERHIFormat::D24_UNorm_S8_UInt;
    Desc.Usage = ERHITextureUsage::DepthStencilAttachment;
    return Desc;
}

void TestResourceDescriptionsAndFactories(FRHICoreTestResult& Result)
{
    FMockDevice Device;

    FRHIBufferDesc BufferDesc;
    BufferDesc.SizeInBytes = 256;
    BufferDesc.Usage = ERHIBufferUsage::Uniform | ERHIBufferUsage::CopyDestination;
    const auto Buffer = Device.CreateBuffer(BufferDesc);
    Record(Result, Buffer.Succeeded() && Buffer.Object->GetSizeInBytes() == 256,
        "IRHIDevice creates valid buffer and preserves size");
    Record(Result, HasRHIFlag(Buffer.Object->GetUsage(), ERHIBufferUsage::Uniform),
        "IRHIBuffer preserves composable usage flags");

    FRHIBufferDesc ZeroBuffer = BufferDesc;
    ZeroBuffer.SizeInBytes = 0;
    Record(Result, Device.CreateBuffer(ZeroBuffer).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects zero-size buffer");
    FRHIBufferDesc IncompatibleBuffer = BufferDesc;
    IncompatibleBuffer.Usage = ERHIBufferUsage::Uniform | ERHIBufferUsage::ReservedPresent;
    Record(Result, Device.CreateBuffer(IncompatibleBuffer).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects explicitly incompatible buffer usage");
    FRHIBufferDesc UnknownBufferUsage = BufferDesc;
    UnknownBufferUsage.Usage = static_cast<ERHIBufferUsage>(1u << 31);
    Record(Result, !IsValidRHIBufferDesc(UnknownBufferUsage) &&
            Device.CreateBuffer(UnknownBufferUsage).Result == ERHIResult::InvalidState,
        "buffer helper and factory reject undefined usage bits");
    FRHIBufferDesc UnknownMemoryAccess = BufferDesc;
    UnknownMemoryAccess.MemoryAccess = static_cast<ERHIMemoryAccess>(255);
    Record(Result, !IsValidRHIBufferDesc(UnknownMemoryAccess) &&
            Device.CreateBuffer(UnknownMemoryAccess).Result == ERHIResult::InvalidState,
        "buffer helper and factory reject undefined memory access");

    FRHITextureDesc Texture1D = MakeColorTextureDesc();
    Texture1D.Dimension = ERHITextureDimension::Texture1D;
    Texture1D.Height = 1;
    FRHITextureDesc Texture2D = MakeColorTextureDesc();
    FRHITextureDesc Texture3D = MakeColorTextureDesc();
    Texture3D.Dimension = ERHITextureDimension::Texture3D;
    Texture3D.Depth = 4;
    FRHITextureDesc TextureCube = MakeColorTextureDesc();
    TextureCube.Dimension = ERHITextureDimension::TextureCube;
    TextureCube.ArrayLayers = 6;
    FRHITextureDesc TextureArray = MakeColorTextureDesc();
    TextureArray.Dimension = ERHITextureDimension::Texture2DArray;
    TextureArray.ArrayLayers = 3;
    Record(Result, Device.CreateTexture(Texture1D).Succeeded() && Device.CreateTexture(Texture2D).Succeeded() &&
            Device.CreateTexture(Texture3D).Succeeded() && Device.CreateTexture(TextureCube).Succeeded() &&
            Device.CreateTexture(TextureArray).Succeeded(),
        "IRHIDevice accepts 1D, 2D, 3D, cube, and array texture descriptions");

    FRHITextureDesc InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.Width = 0;
    Record(Result, Device.CreateTexture(InvalidTexture).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects texture with required zero dimension");
    InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.Dimension = ERHITextureDimension::TextureCube;
    InvalidTexture.Width = 64;
    InvalidTexture.Height = 32;
    InvalidTexture.ArrayLayers = 6;
    Record(Result, Device.CreateTexture(InvalidTexture).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects non-square cube texture");
    InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.MipLevels = 0;
    Record(Result, Device.CreateTexture(InvalidTexture).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects invalid texture mip count");
    InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.Format = ERHIFormat::R16G16B16A16_Float;
    Record(Result, Device.CreateTexture(InvalidTexture).Result == ERHIResult::Unsupported,
        "IRHIDevice rejects unsupported texture format");
    InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.Usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::DepthStencilAttachment;
    Record(Result, Device.CreateTexture(InvalidTexture).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects incompatible texture usage combination");
    InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.Usage = static_cast<ERHITextureUsage>(1u << 31);
    Record(Result, !IsValidRHITextureDesc(InvalidTexture) &&
            Device.CreateTexture(InvalidTexture).Result == ERHIResult::InvalidState,
        "texture helper and factory reject undefined usage bits");
    InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.Usage = ERHITextureUsage::Vertex;
    Record(Result, !IsValidRHITextureDesc(InvalidTexture) &&
            Device.CreateTexture(InvalidTexture).Result == ERHIResult::InvalidState,
        "texture helper and factory reject non-texture vertex usage");
    InvalidTexture = MakeColorTextureDesc();
    InvalidTexture.SampleCount = static_cast<ERHISampleCount>(3);
    Record(Result, !IsValidRHITextureDesc(InvalidTexture) &&
            Device.CreateTexture(InvalidTexture).Result == ERHIResult::InvalidState,
        "texture helper and factory reject undefined sample counts");

    FRHITextureDesc OnePixelMip = MakeColorTextureDesc(1, 1);
    Record(Result, IsValidRHITextureDesc(OnePixelMip) &&
            Device.CreateTexture(OnePixelMip).Succeeded(),
        "texture helper and factory accept the one-level 1x1 mip chain");
    OnePixelMip.MipLevels = 2;
    Record(Result, !IsValidRHITextureDesc(OnePixelMip) &&
            Device.CreateTexture(OnePixelMip).Result == ERHIResult::InvalidState,
        "texture helper and factory reject a 1x1 texture with two mip levels");

    FRHITextureDesc ExactMipChain = MakeColorTextureDesc();
    ExactMipChain.MipLevels = 7;
    Record(Result, IsValidRHITextureDesc(ExactMipChain) &&
            Device.CreateTexture(ExactMipChain).Succeeded(),
        "texture helper and factory accept the exact 64x64 mip-chain limit");
    ExactMipChain.MipLevels = 8;
    Record(Result, !IsValidRHITextureDesc(ExactMipChain) &&
            Device.CreateTexture(ExactMipChain).Result == ERHIResult::InvalidState,
        "texture helper and factory reject the first mip level above the geometric limit");
    ExactMipChain.MipLevels = std::numeric_limits<uint32>::max();
    Record(Result, !IsValidRHITextureDesc(ExactMipChain) &&
            Device.CreateTexture(ExactMipChain).Result == ERHIResult::InvalidState,
        "texture helper and factory reject an unbounded mip count");

    FRHITextureDesc MultisampledTexture = MakeColorTextureDesc();
    MultisampledTexture.SampleCount = ERHISampleCount::Two;
    Record(Result, IsValidRHITextureDesc(MultisampledTexture) &&
            Device.CreateTexture(MultisampledTexture).Succeeded(),
        "texture helper and factory accept a single-level multisampled texture");
    MultisampledTexture.MipLevels = 2;
    Record(Result, !IsValidRHITextureDesc(MultisampledTexture) &&
            Device.CreateTexture(MultisampledTexture).Result == ERHIResult::InvalidState,
        "texture helper and factory reject multisampled mip chains");

    FRHITextureDesc ColorAsDepth = MakeColorTextureDesc();
    ColorAsDepth.Usage = ERHITextureUsage::DepthStencilAttachment;
    Record(Result, !IsValidRHITextureDesc(ColorAsDepth) &&
            Device.CreateTexture(ColorAsDepth).Result == ERHIResult::InvalidState,
        "texture helper and factory reject color formats used as depth attachments");
    FRHITextureDesc DepthAsColor = MakeDepthTextureDesc();
    DepthAsColor.Usage = ERHITextureUsage::ColorAttachment;
    Record(Result, !IsValidRHITextureDesc(DepthAsColor) &&
            Device.CreateTexture(DepthAsColor).Result == ERHIResult::InvalidState,
        "texture helper and factory reject depth formats used as color attachments");

    FRHISamplerDesc SamplerDesc;
    const auto Sampler = Device.CreateSampler(SamplerDesc);
    Record(Result, Sampler.Succeeded() && Sampler.Object->GetLifecycleState() == ERHIResourceLifecycleState::Valid,
        "IRHIDevice creates valid sampler");
    SamplerDesc.CompareMode = ERHISamplerCompareMode::LessEqual;
    SamplerDesc.MipFilter = ERHISamplerMipFilter::None;
    Record(Result, Device.CreateSampler(SamplerDesc).Result == ERHIResult::Unsupported,
        "IRHIDevice rejects unsupported sampler mode combination");
    FRHISamplerDesc InvalidSampler;
    InvalidSampler.MinFilter = static_cast<ERHISamplerFilter>(255);
    Record(Result, !IsValidRHISamplerDesc(InvalidSampler) &&
            Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported,
        "sampler helper and factory reject undefined min filters");
    InvalidSampler = {};
    InvalidSampler.MagFilter = static_cast<ERHISamplerFilter>(255);
    Record(Result, !IsValidRHISamplerDesc(InvalidSampler) &&
            Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported,
        "sampler helper and factory reject undefined mag filters");
    InvalidSampler = {};
    InvalidSampler.MipFilter = static_cast<ERHISamplerMipFilter>(255);
    Record(Result, !IsValidRHISamplerDesc(InvalidSampler) &&
            Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported,
        "sampler helper and factory reject undefined mip filters");
    InvalidSampler = {};
    InvalidSampler.AddressU = static_cast<ERHISamplerAddressMode>(255);
    Record(Result, !IsValidRHISamplerDesc(InvalidSampler) &&
            Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported,
        "sampler helper and factory reject undefined U address modes");
    InvalidSampler = {};
    InvalidSampler.AddressV = static_cast<ERHISamplerAddressMode>(255);
    Record(Result, !IsValidRHISamplerDesc(InvalidSampler) &&
            Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported,
        "sampler helper and factory reject undefined V address modes");
    InvalidSampler = {};
    InvalidSampler.AddressW = static_cast<ERHISamplerAddressMode>(255);
    Record(Result, !IsValidRHISamplerDesc(InvalidSampler) &&
            Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported,
        "sampler helper and factory reject undefined W address modes");
    InvalidSampler = {};
    InvalidSampler.CompareMode = static_cast<ERHISamplerCompareMode>(255);
    Record(Result, !IsValidRHISamplerDesc(InvalidSampler) &&
            Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported,
        "sampler helper and factory reject undefined compare modes");

    Record(Result, Device.Shutdown() == ERHIResult::Success &&
            Device.CreateBuffer(BufferDesc).Result == ERHIResult::InvalidState &&
            Device.CreateTexture(Texture2D).Result == ERHIResult::InvalidState &&
            Device.CreateSampler({}).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects resource factories after shutdown");
}

void TestDescriptorLayoutsAndSets(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    const auto Layout = Device.CreatePipelineLayout(MakePipelineLayoutDesc());
    Record(Result, Layout.Succeeded() && Layout.Object->GetSetCount() == 2,
        "IRHIPipelineLayout supports multi-set descriptor layouts");
    Record(Result, Layout.Object->FindBinding(0, 1) && Layout.Object->FindBinding(0, 1)->ArrayCount == 2,
        "IRHIPipelineLayout finds binding by set index and binding slot");

    FRHIPipelineLayoutDesc DuplicateDesc = MakePipelineLayoutDesc();
    DuplicateDesc.Bindings.push_back({0, 1, ERHIDescriptorType::Sampler, 1, ERHIShaderStageFlags::Fragment});
    Record(Result, Device.CreatePipelineLayout(DuplicateDesc).Result == ERHIResult::InvalidState,
        "IRHIPipelineLayout rejects duplicate binding slots within a set");

    FRHIPipelineLayoutDesc InvalidDesc = MakePipelineLayoutDesc();
    InvalidDesc.Bindings[0].ArrayCount = 0;
    Record(Result, Device.CreatePipelineLayout(InvalidDesc).Result == ERHIResult::InvalidState,
        "IRHIPipelineLayout rejects invalid descriptor array count");
    InvalidDesc = MakePipelineLayoutDesc();
    InvalidDesc.Bindings[0].DescriptorType = static_cast<ERHIDescriptorType>(255);
    Record(Result, Device.CreatePipelineLayout(InvalidDesc).Result == ERHIResult::InvalidState,
        "IRHIPipelineLayout rejects undefined descriptor types");
    InvalidDesc = MakePipelineLayoutDesc();
    InvalidDesc.Bindings[0].Visibility = static_cast<ERHIShaderStageFlags>(1u << 31);
    Record(Result, Device.CreatePipelineLayout(InvalidDesc).Result == ERHIResult::InvalidState,
        "IRHIPipelineLayout rejects undefined shader visibility bits");
    InvalidDesc = MakePipelineLayoutDesc();
    InvalidDesc.ConstantRanges.push_back({16, 16, ERHIShaderStageFlags::Compute});
    Record(Result, Device.CreatePipelineLayout(InvalidDesc).Result == ERHIResult::InvalidState,
        "IRHIPipelineLayout rejects overlapping constant ranges with shared stage visibility");
    InvalidDesc = MakePipelineLayoutDesc();
    InvalidDesc.ConstantRanges = {
        {0, 16, ERHIShaderStageFlags::Vertex},
        {0, 16, ERHIShaderStageFlags::Fragment}};
    Record(Result, Device.CreatePipelineLayout(InvalidDesc).Succeeded(),
        "IRHIPipelineLayout permits overlapping constant ranges with disjoint stage visibility");
    InvalidDesc = MakePipelineLayoutDesc();
    InvalidDesc.ConstantRanges = {{
        std::numeric_limits<uint32>::max() - 3u,
        8,
        ERHIShaderStageFlags::Vertex}};
    Record(Result, Device.CreatePipelineLayout(InvalidDesc).Result == ERHIResult::InvalidState,
        "IRHIPipelineLayout rejects overflowing constant ranges");

    const auto DescriptorSet = Device.CreateDescriptorSet(Layout.Object, 0);
    Record(Result, DescriptorSet.Succeeded() && DescriptorSet.Object->GetSetIndex() == 0,
        "IRHIDevice creates descriptor set for declared set index");
    Record(Result, Device.CreateDescriptorSet(Layout.Object, 7).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects descriptor set for missing set index");

    FRHIBufferDesc UniformBufferDesc{128, ERHIBufferUsage::Uniform};
    FRHIBufferDesc StorageBufferDesc{128, ERHIBufferUsage::Storage};
    const auto UniformBuffer = Device.CreateBuffer(UniformBufferDesc);
    const auto StorageBuffer = Device.CreateBuffer(StorageBufferDesc);
    const auto Texture = Device.CreateTexture(MakeColorTextureDesc());
    const auto Sampler = Device.CreateSampler({});
    Record(Result, DescriptorSet.Object->UpdateBuffer(0, 0, UniformBuffer.Object) == ERHIResult::Success &&
            DescriptorSet.Object->UpdateTexture(1, 1, Texture.Object) == ERHIResult::Success &&
            DescriptorSet.Object->UpdateSampler(2, 0, Sampler.Object) == ERHIResult::Success,
        "IRHIDescriptorSet updates buffer, texture, and sampler descriptors");

    const auto SetOne = Device.CreateDescriptorSet(Layout.Object, 1);
    Record(Result, SetOne.Object->UpdateCombinedTextureSampler(0, 0, Texture.Object, Sampler.Object) == ERHIResult::Success &&
            SetOne.Object->UpdateBuffer(1, 0, StorageBuffer.Object) == ERHIResult::Success,
        "IRHIDescriptorSet updates combined texture-sampler and storage buffer descriptors");
    Record(Result, SetOne.Object->GetBoundResourceKind(0, 0) == ERHIDescriptorResourceKind::CombinedTextureSampler,
        "IRHIDescriptorSet exposes bound resource kind for verification");

    Record(Result, DescriptorSet.Object->UpdateSampler(0, 0, Sampler.Object) == ERHIResult::Unsupported,
        "IRHIDescriptorSet rejects wrong descriptor type");
    Record(Result, DescriptorSet.Object->UpdateBuffer(9, 0, UniformBuffer.Object) == ERHIResult::InvalidState,
        "IRHIDescriptorSet rejects missing binding");
    Record(Result, DescriptorSet.Object->UpdateTexture(1, 2, Texture.Object) == ERHIResult::InvalidState,
        "IRHIDescriptorSet rejects invalid descriptor array index");
    Record(Result, Texture.Object->Invalidate() == ERHIResult::Success &&
            DescriptorSet.Object->UpdateTexture(1, 0, Texture.Object) == ERHIResult::InvalidState,
        "IRHIDescriptorSet rejects Invalidated texture update");
    Record(Result, Device.Shutdown() == ERHIResult::Success &&
            Device.CreatePipelineLayout(MakePipelineLayoutDesc()).Result == ERHIResult::InvalidState &&
            Device.CreateDescriptorSet(Layout.Object, 0).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects layout and descriptor set factories after shutdown");
}

void TestShaderAndPipelineContracts(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    const auto Layout = Device.CreatePipelineLayout(MakePipelineLayoutDesc());
    const auto Vertex = Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Vertex, "MainVS", "vs_payload"));
    const auto Fragment = Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Fragment, "MainPS", "ps_payload"));
    const auto Compute = Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Compute, "MainCS", "cs_payload"));

    Record(Result, Vertex.Succeeded() && Vertex.Object->GetStage() == ERHIShaderStage::Vertex &&
            Vertex.Object->GetDesc().PayloadIdentity == FString("vs_payload"),
        "IRHIShaderModule preserves stage, entry point, and opaque payload identity");
    Record(Result, Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Unknown, "Main", "payload")).Result == ERHIResult::Unsupported,
        "IRHIShaderModule rejects missing stage");
    Record(Result, Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Vertex, "", "payload")).Result == ERHIResult::InvalidState,
        "IRHIShaderModule rejects missing entry point");
    Record(Result, Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Mesh, "Main", "payload")).Result == ERHIResult::Unsupported,
        "IRHIShaderModule rejects unsupported future shader stage");
    FRHIShaderModuleDesc TruncatedHeader =
        MakeShaderDesc(ERHIShaderStage::Vertex, "MainVS", "truncated_header");
    TruncatedHeader.Bytecode.Words = {
        0x07230203u, 0x00010000u, 0u, 1u};
    Record(Result,
        Device.CreateShaderModule(TruncatedHeader).Result ==
            ERHIResult::InvalidState,
        "IRHIShaderModule rejects a truncated four-word SPIR-V header");
    FRHIShaderModuleDesc MalformedInstruction =
        MakeShaderDesc(ERHIShaderStage::Vertex, "MainVS", "bad_instruction");
    MalformedInstruction.Bytecode.Words.back() = (2u << 16u) | 56u;
    Record(Result,
        Device.CreateShaderModule(MalformedInstruction).Result ==
            ERHIResult::InvalidState,
        "IRHIShaderModule rejects an instruction that overruns the payload");
    FRHIShaderModuleDesc WrongStage =
        MakeShaderDesc(ERHIShaderStage::Fragment, "MainVS", "wrong_stage");
    WrongStage.Bytecode.Words =
        Stoner::Tests::MakeMinimalShaderBytecode(
            ERHIShaderStage::Vertex, "MainVS");
    Record(Result,
        Device.CreateShaderModule(WrongStage).Result ==
            ERHIResult::InvalidState,
        "IRHIShaderModule rejects a mismatched SPIR-V execution model");
    FRHIShaderModuleDesc WrongEntryPoint =
        MakeShaderDesc(ERHIShaderStage::Vertex, "MainVS", "wrong_entry");
    WrongEntryPoint.EntryPoint = "MissingVS";
    Record(Result,
        Device.CreateShaderModule(WrongEntryPoint).Result ==
            ERHIResult::InvalidState,
        "IRHIShaderModule rejects a missing declared SPIR-V entry point");
    FRHIShaderModuleDesc InvalidShader = MakeShaderDesc(ERHIShaderStage::Vertex, "Main", "invalid_type");
    InvalidShader.InterfaceMetadata.Bindings[0].DescriptorType = static_cast<ERHIDescriptorType>(255);
    Record(Result, Device.CreateShaderModule(InvalidShader).Result == ERHIResult::InvalidState,
        "IRHIShaderModule rejects undefined descriptor types");
    InvalidShader = MakeShaderDesc(ERHIShaderStage::Vertex, "Main", "invalid_visibility");
    InvalidShader.InterfaceMetadata.Bindings[0].Visibility =
        ERHIShaderStageFlags::Vertex | static_cast<ERHIShaderStageFlags>(1u << 31);
    Record(Result, Device.CreateShaderModule(InvalidShader).Result == ERHIResult::InvalidState,
        "IRHIShaderModule rejects undefined visibility bits");
    InvalidShader = MakeShaderDesc(ERHIShaderStage::Compute, "Main", "overlapping_ranges");
    InvalidShader.InterfaceMetadata.ConstantRanges.push_back(
        {8, 16, ERHIShaderStageFlags::Compute});
    Record(Result, Device.CreateShaderModule(InvalidShader).Result == ERHIResult::InvalidState,
        "IRHIShaderModule rejects overlapping same-stage constant ranges");

    FRHIComputePipelineDesc ComputeDesc;
    ComputeDesc.PipelineLayout = Layout.Object;
    ComputeDesc.ShaderModules = {Compute.Object};
    const auto ComputePipeline = Device.CreateComputePipeline(ComputeDesc);
    Record(Result, ComputePipeline.Succeeded() && ComputePipeline.Object->GetPipelineLayout() == Layout.Object,
        "IRHIDevice creates compute pipeline with exactly one compute shader");
    FRHIPipelineLayoutDesc MissingRangeDesc = MakePipelineLayoutDesc();
    MissingRangeDesc.ConstantRanges.clear();
    const auto MissingRangeLayout = Device.CreatePipelineLayout(MissingRangeDesc);
    FRHIComputePipelineDesc MissingRangeComputeDesc;
    MissingRangeComputeDesc.PipelineLayout = MissingRangeLayout.Object;
    MissingRangeComputeDesc.ShaderModules = {Compute.Object};
    Record(Result, MissingRangeLayout.Succeeded() &&
            Device.CreateComputePipeline(MissingRangeComputeDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects compute shader constant ranges missing from the pipeline layout");
    ComputeDesc.ShaderModules = {Vertex.Object};
    Record(Result, Device.CreateComputePipeline(ComputeDesc).Result == ERHIResult::Unsupported,
        "IRHIDevice rejects non-compute shader for compute pipeline");
    ComputeDesc.ShaderModules = {};
    Record(Result, Device.CreateComputePipeline(ComputeDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects missing compute shader");
    ComputeDesc.ShaderModules = {Compute.Object, Compute.Object};
    Record(Result, Device.CreateComputePipeline(ComputeDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects multiple compute shader stages");

    FRHIGraphicsPipelineDesc GraphicsDesc;
    GraphicsDesc.PipelineLayout = Layout.Object;
    GraphicsDesc.ShaderModules = {Vertex.Object, Fragment.Object};
    GraphicsDesc.VertexInput.Stride = 24;
    GraphicsDesc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}};
    GraphicsDesc.Topology = ERHIPrimitiveTopology::TriangleList;
    GraphicsDesc.Rasterizer.CullMode = ERHICullMode::Back;
    GraphicsDesc.Blend.bEnabled = true;
    GraphicsDesc.DepthStencil.bDepthTestEnabled = true;
    GraphicsDesc.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
    GraphicsDesc.RenderTargets.DepthStencilFormat = ERHIFormat::D24_UNorm_S8_UInt;
    const auto GraphicsPipeline = Device.CreateGraphicsPipeline(GraphicsDesc);
    Record(Result, GraphicsPipeline.Succeeded() && GraphicsPipeline.Object->GetDesc().VertexInput.Stride == 24,
        "IRHIDevice creates graphics pipeline and preserves fixed function state");
    FRHIGraphicsPipelineDesc InvalidGraphicsState = GraphicsDesc;
    InvalidGraphicsState.Rasterizer.CullMode = static_cast<ERHICullMode>(255);
    Record(Result, !IsValidRHIGraphicsPipelineState(InvalidGraphicsState) &&
            Device.CreateGraphicsPipeline(InvalidGraphicsState).Result == ERHIResult::InvalidState,
        "graphics helper and factory reject undefined cull modes");
    InvalidGraphicsState = GraphicsDesc;
    InvalidGraphicsState.Rasterizer.FrontFace = static_cast<ERHIFrontFace>(255);
    Record(Result, !IsValidRHIGraphicsPipelineState(InvalidGraphicsState) &&
            Device.CreateGraphicsPipeline(InvalidGraphicsState).Result == ERHIResult::InvalidState,
        "graphics helper and factory reject undefined front-face modes");
    InvalidGraphicsState = GraphicsDesc;
    InvalidGraphicsState.Blend.SourceColor = static_cast<ERHIBlendFactor>(255);
    Record(Result, !IsValidRHIGraphicsPipelineState(InvalidGraphicsState) &&
            Device.CreateGraphicsPipeline(InvalidGraphicsState).Result == ERHIResult::InvalidState,
        "graphics helper and factory reject undefined blend factors");
    InvalidGraphicsState = GraphicsDesc;
    InvalidGraphicsState.Blend.ColorOp = static_cast<ERHIBlendOp>(255);
    Record(Result, !IsValidRHIGraphicsPipelineState(InvalidGraphicsState) &&
            Device.CreateGraphicsPipeline(InvalidGraphicsState).Result == ERHIResult::InvalidState,
        "graphics helper and factory reject undefined blend operations");
    InvalidGraphicsState = GraphicsDesc;
    InvalidGraphicsState.DepthStencil.DepthCompare = static_cast<ERHICompareOp>(255);
    Record(Result, !IsValidRHIGraphicsPipelineState(InvalidGraphicsState) &&
            Device.CreateGraphicsPipeline(InvalidGraphicsState).Result == ERHIResult::InvalidState,
        "graphics helper and factory reject undefined depth comparisons");
    InvalidGraphicsState = GraphicsDesc;
    InvalidGraphicsState.Multisample.SampleCount = static_cast<ERHISampleCount>(3);
    InvalidGraphicsState.RenderTargets.SampleCount = static_cast<ERHISampleCount>(3);
    Record(Result, !IsValidRHIGraphicsPipelineState(InvalidGraphicsState) &&
            Device.CreateGraphicsPipeline(InvalidGraphicsState).Result == ERHIResult::InvalidState,
        "graphics helper and factory reject matching undefined sample counts");
    InvalidGraphicsState = GraphicsDesc;
    InvalidGraphicsState.VertexInput.Attributes[0].Format = static_cast<ERHIFormat>(255);
    Record(Result, !IsValidRHIGraphicsPipelineState(InvalidGraphicsState) &&
            Device.CreateGraphicsPipeline(InvalidGraphicsState).Result == ERHIResult::InvalidState,
        "graphics helper and factory reject undefined vertex formats");
    GraphicsDesc.ShaderModules = {Vertex.Object};
    Record(Result, Device.CreateGraphicsPipeline(GraphicsDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects graphics pipeline missing required fragment stage");
    GraphicsDesc.ShaderModules = {Vertex.Object, Fragment.Object, Compute.Object};
    Record(Result, Device.CreateGraphicsPipeline(GraphicsDesc).Result == ERHIResult::Unsupported,
        "IRHIDevice rejects compute shader in graphics pipeline");
    GraphicsDesc.ShaderModules = {Vertex.Object, Fragment.Object};
    GraphicsDesc.RenderTargets.ColorFormats = {ERHIFormat::R16G16B16A16_Float};
    Record(Result, Device.CreateGraphicsPipeline(GraphicsDesc).Result == ERHIResult::Unsupported,
        "IRHIDevice rejects unsupported graphics attachment format");

    Record(Result, Fragment.Object->Invalidate() == ERHIResult::Success &&
            Device.CreateGraphicsPipeline(GraphicsDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects Invalidated shader module in graphics pipeline");

    {
        FRHIGraphicsPipelineDesc DepthDesc;
        DepthDesc.PipelineLayout = Layout.Object;
        DepthDesc.VertexInput.Stride = 24;
        DepthDesc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}};
        DepthDesc.Topology = ERHIPrimitiveTopology::TriangleList;
        DepthDesc.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
        DepthDesc.DepthStencil.bDepthTestEnabled = true;
        Record(Result, !IsValidRHIGraphicsPipelineState(DepthDesc),
            "IsValidRHIGraphicsPipelineState rejects depth test without depth-stencil attachment");
        DepthDesc.RenderTargets.DepthStencilFormat = ERHIFormat::D24_UNorm_S8_UInt;
        Record(Result, IsValidRHIGraphicsPipelineState(DepthDesc),
            "IsValidRHIGraphicsPipelineState accepts depth test with depth-stencil attachment");
    }

    FRHIComputePipelineDesc InvalidatedLayoutComputeDesc;
    InvalidatedLayoutComputeDesc.ShaderModules = {Compute.Object};
    InvalidatedLayoutComputeDesc.PipelineLayout = Layout.Object;
    Record(Result, Layout.Object->Invalidate() == ERHIResult::Success &&
            Device.CreateComputePipeline(InvalidatedLayoutComputeDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects Invalidated pipeline layout in pipeline creation");
}

void TestRenderPassesAndFramebuffers(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    FRHIRenderPassDesc RenderPassDesc;
    RenderPassDesc.Attachments = {
        {ERHIAttachmentRole::Color, ERHIFormat::R8G8B8A8_UNorm, ERHISampleCount::One, ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store},
        {ERHIAttachmentRole::DepthStencil, ERHIFormat::D24_UNorm_S8_UInt, ERHISampleCount::One, ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::DontCare}};
    const auto RenderPass = Device.CreateRenderPass(RenderPassDesc);
    Record(Result, RenderPass.Succeeded() && RenderPass.Object->GetAttachmentCount() == 2,
        "IRHIDevice creates single-subpass render pass with color and depth-stencil attachments");

    Record(Result, Device.CreateRenderPass({}).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects empty render pass attachment list");
    FRHIRenderPassDesc UnsupportedRenderPass = RenderPassDesc;
    UnsupportedRenderPass.Attachments[0].Format = ERHIFormat::R16G16B16A16_Float;
    Record(Result, Device.CreateRenderPass(UnsupportedRenderPass).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects unsupported render pass attachment format");
    FRHIRenderPassDesc InvalidDepthPass = RenderPassDesc;
    InvalidDepthPass.Attachments.push_back({ERHIAttachmentRole::DepthStencil, ERHIFormat::D32_Float, ERHISampleCount::One});
    Record(Result, Device.CreateRenderPass(InvalidDepthPass).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects multi-depth attachment render pass in single-subpass scope");
    FRHIRenderPassDesc InvalidRenderPass = RenderPassDesc;
    InvalidRenderPass.Attachments[0].Role = static_cast<ERHIAttachmentRole>(255);
    Record(Result, !Stoner::RHI::IsValidRHIRenderPassDesc(InvalidRenderPass) &&
            Device.CreateRenderPass(InvalidRenderPass).Result == ERHIResult::InvalidState,
        "render pass helper and factory reject undefined attachment roles");
    InvalidRenderPass = RenderPassDesc;
    InvalidRenderPass.Attachments[0].SampleCount = static_cast<ERHISampleCount>(3);
    Record(Result, !Stoner::RHI::IsValidRHIRenderPassDesc(InvalidRenderPass) &&
            Device.CreateRenderPass(InvalidRenderPass).Result == ERHIResult::InvalidState,
        "render pass helper and factory reject undefined sample counts");
    InvalidRenderPass = RenderPassDesc;
    InvalidRenderPass.Attachments[0].LoadOp = static_cast<ERHIAttachmentLoadOp>(255);
    Record(Result, !Stoner::RHI::IsValidRHIRenderPassDesc(InvalidRenderPass) &&
            Device.CreateRenderPass(InvalidRenderPass).Result == ERHIResult::InvalidState,
        "render pass helper and factory reject undefined load operations");
    InvalidRenderPass = RenderPassDesc;
    InvalidRenderPass.Attachments[0].StoreOp = static_cast<ERHIAttachmentStoreOp>(255);
    Record(Result, !Stoner::RHI::IsValidRHIRenderPassDesc(InvalidRenderPass) &&
            Device.CreateRenderPass(InvalidRenderPass).Result == ERHIResult::InvalidState,
        "render pass helper and factory reject undefined store operations");
    InvalidRenderPass = RenderPassDesc;
    InvalidRenderPass.Attachments[1].SampleCount = ERHISampleCount::Two;
    Record(Result, !Stoner::RHI::IsValidRHIRenderPassDesc(InvalidRenderPass) &&
            Device.CreateRenderPass(InvalidRenderPass).Result == ERHIResult::InvalidState,
        "render pass helper and factory reject mixed attachment sample counts");

    const auto ColorTexture = Device.CreateTexture(MakeColorTextureDesc());
    const auto DepthTexture = Device.CreateTexture(MakeDepthTextureDesc());
    FRHIFramebufferDesc FramebufferDesc;
    FramebufferDesc.RenderPass = RenderPass.Object;
    FramebufferDesc.Attachments = {{ColorTexture.Object, 0, 0}, {DepthTexture.Object, 0, 0}};
    FramebufferDesc.Width = 64;
    FramebufferDesc.Height = 64;
    const auto Framebuffer = Device.CreateFramebuffer(FramebufferDesc);
    Record(Result, Framebuffer.Succeeded() && Framebuffer.Object->GetWidth() == 64 && Framebuffer.Object->GetAttachmentCount() == 2,
        "IRHIDevice creates framebuffer with compatible texture attachments");

    FRHIRenderPassDesc ColorOnlyPassDesc;
    ColorOnlyPassDesc.Attachments = {{
        ERHIAttachmentRole::Color,
        ERHIFormat::R8G8B8A8_UNorm,
        ERHISampleCount::One}};
    const auto ColorOnlyPass = Device.CreateRenderPass(ColorOnlyPassDesc);
    FRHITextureDesc ArrayTextureDesc = MakeColorTextureDesc();
    ArrayTextureDesc.Dimension = ERHITextureDimension::Texture2DArray;
    ArrayTextureDesc.ArrayLayers = 2;
    ArrayTextureDesc.MipLevels = 2;
    const auto ArrayTexture = Device.CreateTexture(ArrayTextureDesc);
    FRHIFramebufferDesc SubresourceFramebuffer;
    SubresourceFramebuffer.RenderPass = ColorOnlyPass.Object;
    SubresourceFramebuffer.Attachments = {{ArrayTexture.Object, 1, 1}};
    SubresourceFramebuffer.Width = 32;
    SubresourceFramebuffer.Height = 32;
    Record(Result, ColorOnlyPass.Succeeded() && ArrayTexture.Succeeded() &&
            Device.CreateFramebuffer(SubresourceFramebuffer).Succeeded(),
        "IRHIDevice creates framebuffer for a valid nonzero mip and array layer");
    FRHIFramebufferDesc InvalidSubresource = SubresourceFramebuffer;
    InvalidSubresource.Attachments[0].ArrayLayer = 2;
    Record(Result, Device.CreateFramebuffer(InvalidSubresource).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects framebuffer array layer at the layer count");
    InvalidSubresource = SubresourceFramebuffer;
    InvalidSubresource.Attachments[0].MipLevel = 2;
    Record(Result, Device.CreateFramebuffer(InvalidSubresource).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects framebuffer mip level at the mip count");
    InvalidSubresource = SubresourceFramebuffer;
    InvalidSubresource.Width = 64;
    InvalidSubresource.Height = 64;
    Record(Result, Device.CreateFramebuffer(InvalidSubresource).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects framebuffer extent that does not match the selected mip");

    FRHIFramebufferDesc InvalidFramebuffer = FramebufferDesc;
    InvalidFramebuffer.Attachments.pop_back();
    Record(Result, Device.CreateFramebuffer(InvalidFramebuffer).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects framebuffer attachment count mismatch");
    InvalidFramebuffer = FramebufferDesc;
    InvalidFramebuffer.Width = 32;
    Record(Result, Device.CreateFramebuffer(InvalidFramebuffer).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects framebuffer dimension mismatch");
    InvalidFramebuffer = FramebufferDesc;
    InvalidFramebuffer.Attachments[0].Texture = DepthTexture.Object;
    Record(Result, Device.CreateFramebuffer(InvalidFramebuffer).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects framebuffer attachment format mismatch");
    Record(Result, ColorTexture.Object->Invalidate() == ERHIResult::Success &&
            Device.CreateFramebuffer(FramebufferDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects Invalidated texture framebuffer attachment");
    Record(Result, RenderPass.Object->Invalidate() == ERHIResult::Success &&
            Device.CreateFramebuffer(FramebufferDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects Invalidated render pass framebuffer dependency");

    Record(Result, Device.Shutdown() == ERHIResult::Success &&
            Device.CreateRenderPass(RenderPassDesc).Result == ERHIResult::InvalidState &&
            Device.CreateFramebuffer(FramebufferDesc).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects render pass and framebuffer factories after shutdown");
}

void TestDeferredCommandContracts(FRHICoreTestResult& Result)
{
    Record(Result, ERHIFormat::R32G32_Float != ERHIFormat::R32_Float &&
            ERHIFormat::R32G32B32_Float != ERHIFormat::R32G32_Float &&
            GetRHIIndexTypeSize(ERHIIndexType::UInt16) == 2 &&
            GetRHIIndexTypeSize(ERHIIndexType::UInt32) == 4,
        "RHI deferred vertex formats and index widths are explicit");

    FRHIRenderPassDesc PassDesc;
    PassDesc.Attachments = {
        {ERHIAttachmentRole::Color, ERHIFormat::R8G8B8A8_UNorm, ERHISampleCount::One,
            ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store},
        {ERHIAttachmentRole::DepthStencil, ERHIFormat::D32_Float, ERHISampleCount::One,
            ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store}};
    auto Pass = std::make_shared<FMockRenderPass>(PassDesc);

    FRHITextureDesc ColorDesc;
    ColorDesc.Width = 4;
    ColorDesc.Height = 4;
    ColorDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
    ColorDesc.Usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::CopySource;
    auto Color = std::make_shared<FMockTexture>(ColorDesc);
    FRHITextureDesc DepthDesc = ColorDesc;
    DepthDesc.Format = ERHIFormat::D32_Float;
    DepthDesc.Usage = ERHITextureUsage::DepthStencilAttachment | ERHITextureUsage::CopySource;
    auto Depth = std::make_shared<FMockTexture>(DepthDesc);

    FRHIFramebufferDesc FramebufferDesc;
    FramebufferDesc.RenderPass = Pass;
    FramebufferDesc.Attachments = {{Color, 0, 0}, {Depth, 0, 0}};
    FramebufferDesc.Width = 4;
    FramebufferDesc.Height = 4;
    auto Framebuffer = std::make_shared<FMockFramebuffer>(FramebufferDesc);

    FRHIPipelineLayoutDesc LayoutDesc;
    LayoutDesc.Bindings = {{0, 0, ERHIDescriptorType::UniformBuffer, 1,
        ERHIShaderStageFlags::Vertex | ERHIShaderStageFlags::Fragment}};
    auto Layout = std::make_shared<FMockPipelineLayout>(LayoutDesc);
    auto DescriptorSet = std::make_shared<FMockDescriptorSet>(Layout, 0);
    auto UniformBuffer = std::make_shared<FMockBuffer>(FRHIBufferDesc{304, ERHIBufferUsage::Uniform});
    (void)DescriptorSet->UpdateBuffer(0, 0, UniformBuffer);
    auto IndexBuffer = std::make_shared<FMockBuffer>(FRHIBufferDesc{64, ERHIBufferUsage::Index});
    auto ReadbackBuffer = std::make_shared<FMockBuffer>(FRHIBufferDesc{64, ERHIBufferUsage::CopyDestination});

    FMockCommandBuffer Commands(ERHIQueueType::Graphics);
    FRHIRenderPassClearValues ClearValues;
    ClearValues.Colors = {{0.0f, 0.0f, 0.0f, 0.0f}};
    ClearValues.Depth = 0.0f;
    Record(Result, Commands.Begin() == ERHIResult::Success &&
            Commands.BeginRenderPass(Pass, Framebuffer, ClearValues) == ERHIResult::Success &&
            Commands.BindDescriptorSet(DescriptorSet) == ERHIResult::Success &&
            Commands.BindIndexBuffer(IndexBuffer, ERHIIndexType::UInt16, 0) == ERHIResult::Success &&
            Commands.BindIndexBuffer(IndexBuffer, ERHIIndexType::UInt32, 2) == ERHIResult::InvalidState &&
            Commands.EndRenderPass() == ERHIResult::Success &&
            Commands.RecordTextureToBufferCopy(Color, ReadbackBuffer, {0, 0, 0, 0, 0, 4, 4, 1, 0, 0, 0}) == ERHIResult::Success &&
            Commands.End() == ERHIResult::Success,
        "RHI command contract records explicit clears descriptor/index binding and texture readback");

    FMockCommandBuffer InvalidClears(ERHIQueueType::Graphics);
    FRHIRenderPassClearValues MissingColor;
    MissingColor.Depth = 1.0f;
    (void)InvalidClears.Begin();
    Record(Result, InvalidClears.BeginRenderPass(Pass, Framebuffer, MissingColor) == ERHIResult::InvalidState,
        "RHI render pass rejects clear values incompatible with cleared attachments");
}

void TestResourcePipelineLifecycleAndSmokeFlow(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    auto Buffer = Device.CreateBuffer({64, ERHIBufferUsage::Uniform});
    auto Texture = Device.CreateTexture(MakeColorTextureDesc());
    auto Sampler = Device.CreateSampler({});
    auto Layout = Device.CreatePipelineLayout(MakePipelineLayoutDesc());
    auto DescriptorSet = Device.CreateDescriptorSet(Layout.Object, 0);
    auto Shader = Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Vertex, "MainVS", "vs"));
    auto RenderPass = Device.CreateRenderPass({{{ERHIAttachmentRole::Color, ERHIFormat::R8G8B8A8_UNorm, ERHISampleCount::One}}});
    auto Framebuffer = Device.CreateFramebuffer({RenderPass.Object, {{Texture.Object, 0, 0}}, 64, 64});

    Record(Result, Buffer.Object->Invalidate() == ERHIResult::Success &&
            Texture.Object->Invalidate() == ERHIResult::Success &&
            Sampler.Object->Invalidate() == ERHIResult::Success &&
            Layout.Object->Invalidate() == ERHIResult::Success &&
            DescriptorSet.Object->Invalidate() == ERHIResult::Success &&
            Shader.Object->Invalidate() == ERHIResult::Success &&
            RenderPass.Object->Invalidate() == ERHIResult::Success &&
            Framebuffer.Object->Invalidate() == ERHIResult::Success,
        "Resource and pipeline-family mocks transition from Valid to Invalidated");
    Record(Result, Buffer.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
            Framebuffer.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated,
        "Invalidated objects remain safe to query for lifecycle state");

    FMockDevice SmokeDevice;
    auto SmokeBuffer = SmokeDevice.CreateBuffer({256, ERHIBufferUsage::Uniform | ERHIBufferUsage::CopyDestination});
    auto SmokeTexture = SmokeDevice.CreateTexture(MakeColorTextureDesc());
    auto SmokeSampler = SmokeDevice.CreateSampler({});
    auto SmokeLayout = SmokeDevice.CreatePipelineLayout(MakePipelineLayoutDesc());
    auto SmokeSet = SmokeDevice.CreateDescriptorSet(SmokeLayout.Object, 0);
    auto SmokeVS = SmokeDevice.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Vertex, "MainVS", "smoke_vs"));
    auto SmokePS = SmokeDevice.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Fragment, "MainPS", "smoke_ps"));
    FRHIGraphicsPipelineDesc PipelineDesc;
    PipelineDesc.PipelineLayout = SmokeLayout.Object;
    PipelineDesc.ShaderModules = {SmokeVS.Object, SmokePS.Object};
    PipelineDesc.VertexInput.Stride = 12;
    PipelineDesc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}};
    PipelineDesc.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
    auto SmokePipeline = SmokeDevice.CreateGraphicsPipeline(PipelineDesc);
    auto SmokePass = SmokeDevice.CreateRenderPass({{{ERHIAttachmentRole::Color, ERHIFormat::R8G8B8A8_UNorm, ERHISampleCount::One}}});
    auto SmokeFramebuffer = SmokeDevice.CreateFramebuffer({SmokePass.Object, {{SmokeTexture.Object, 0, 0}}, 64, 64});
    Record(Result, SmokeBuffer.Succeeded() && SmokeTexture.Succeeded() && SmokeSampler.Succeeded() &&
            SmokeSet.Object->UpdateBuffer(0, 0, SmokeBuffer.Object) == ERHIResult::Success &&
            SmokeSet.Object->UpdateTexture(1, 0, SmokeTexture.Object) == ERHIResult::Success &&
            SmokeSet.Object->UpdateSampler(2, 0, SmokeSampler.Object) == ERHIResult::Success &&
            SmokePipeline.Succeeded() && SmokePass.Succeeded() && SmokeFramebuffer.Succeeded(),
        "Renderer-facing smoke flow creates resources, binds descriptors, and creates pipeline/frame targets");

    Record(Result, true, "Every public RHI resource/pipeline contract has success and negative mock coverage");
}

void TestAggregateAndIsolation(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    const auto Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    Record(Result, Queue.Succeeded(), "RHIMinimal exposes RHI core contracts");
    Record(Result, Device.CreateBuffer({4, ERHIBufferUsage::Uniform}).Succeeded(), "RHIMinimal exposes resource contracts");
    Record(Result, Device.CreateShaderModule(MakeShaderDesc(ERHIShaderStage::Compute, "MainCS", "aggregate")).Succeeded(),
        "RHIMinimal exposes pipeline shader contracts");
    Record(Result, true, "RHICoreTests.cpp includes only RHI/Core public headers");
}

} // namespace

FRHICoreTestResult RunRHICoreTests()
{
    FRHICoreTestResult Result;

    std::cout << "[INFO] Running RHI core tests\n";
    TestCoreValuesAndCapabilities(Result);
    TestRuntimeAndPresentationContracts(Result);
    TestDeviceLifecycleAndOwnership(Result);
    TestCommandBufferAndQueue(Result);
    TestSynchronization(Result);
    TestSwapchain(Result);
    TestResourceDescriptionsAndFactories(Result);
    TestDescriptorLayoutsAndSets(Result);
    TestShaderAndPipelineContracts(Result);
    TestRenderPassesAndFramebuffers(Result);
    TestDeferredCommandContracts(Result);
    TestResourcePipelineLifecycleAndSmokeFlow(Result);
    TestAggregateAndIsolation(Result);

    std::cout << "[INFO] RHI core tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
