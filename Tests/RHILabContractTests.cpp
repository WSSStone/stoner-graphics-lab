#include "RHI/RHIMinimal.h"

#include <iostream>
#include <memory>

namespace
{

using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FRHILabContractTestResult
{
    int Passed = 0;
    int Failed = 0;
};

void Record(FRHILabContractTestResult& Result, bool bPassed, const char* Name)
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

class FTestFence final : public IRHIFence
{
public:
    [[nodiscard]] ERHIFenceState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] bool IsSignaled() const noexcept override
    {
        return State == ERHIFenceState::Signaled ||
            State == ERHIFenceState::Waited;
    }

    ERHIResult Wait(uint64 TimeoutMicroseconds = 0) override
    {
        if (!IsSignaled())
        {
            return TimeoutMicroseconds > 0
                ? ERHIResult::Timeout
                : ERHIResult::NotReady;
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

class FTestSemaphore final : public IRHISemaphore
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
        if (IsSignaled())
        {
            return ERHIResult::InvalidState;
        }
        State = ERHISemaphoreState::Signaled;
        return ERHIResult::Success;
    }

    ERHIResult Consume() override
    {
        if (!IsSignaled())
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

class FTestTexture final : public IRHITexture
{
public:
    explicit FTestTexture(
        uint32 Width = 640,
        uint32 Height = 360,
        ERHIFormat Format = ERHIFormat::R8G8B8A8_UNorm)
    {
        Desc.Width = Width;
        Desc.Height = Height;
        Desc.Format = Format;
        Desc.Usage = ERHITextureUsage::ColorAttachment |
            ERHITextureUsage::Present;
    }

    [[nodiscard]] const FRHITextureDesc& GetDesc() const noexcept override
    {
        return Desc;
    }

    [[nodiscard]] ERHITextureDimension GetDimension() const noexcept override
    {
        return Desc.Dimension;
    }

    [[nodiscard]] ERHIFormat GetFormat() const noexcept override
    {
        return Desc.Format;
    }

    [[nodiscard]] ERHITextureUsage GetUsage() const noexcept override
    {
        return Desc.Usage;
    }

    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return State;
    }

    ERHIResult Invalidate() override
    {
        State = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

private:
    FRHITextureDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FTestCommandBuffer final : public IRHICommandBuffer
{
public:
    [[nodiscard]] ERHICommandBufferState GetState() const noexcept override
    {
        return ERHICommandBufferState::Completed;
    }

    [[nodiscard]] ERHIQueueType GetCompatibleQueueType() const noexcept override
    {
        return ERHIQueueType::Graphics;
    }

    [[nodiscard]] uint32 GetRecordedCommandCount() const noexcept override
    {
        return 1;
    }

    ERHIResult Begin() override { return ERHIResult::Unsupported; }
    ERHIResult End() override { return ERHIResult::Unsupported; }
    ERHIResult Reset() override { return ERHIResult::Unsupported; }
    ERHIResult RecordDraw(uint32, uint32) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordDispatch(uint32, uint32, uint32) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult BindGraphicsPipeline(
        const TSharedPtr<IRHIGraphicsPipeline>&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult BindComputePipeline(
        const TSharedPtr<IRHIComputePipeline>&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordBarrier() override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordBufferCopy(
        const TSharedPtr<IRHIBuffer>&,
        const TSharedPtr<IRHIBuffer>&,
        FRHIBufferCopyRange) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordTextureCopy(
        const TSharedPtr<IRHITexture>&,
        const TSharedPtr<IRHITexture>&,
        FRHITextureCopyRegion) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult BeginRenderPass(
        const TSharedPtr<IRHIRenderPass>&,
        const TSharedPtr<IRHIFramebuffer>&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult EndRenderPass() override { return ERHIResult::Unsupported; }
};

class FTrackingCommandQueue final : public IRHICommandQueue
{
public:
    [[nodiscard]] ERHIQueueType GetQueueType() const noexcept override
    {
        return ERHIQueueType::Graphics;
    }

    [[nodiscard]] uint32 GetSubmittedCommandBufferCount() const noexcept override
    {
        return SubmittedCommandBufferCount;
    }

    ERHIResult Submit(
        const TSharedPtr<IRHICommandBuffer>&,
        const TArray<TSharedPtr<IRHISemaphore>>&,
        const TArray<TSharedPtr<IRHISemaphore>>&,
        const TSharedPtr<IRHIFence>&) override
    {
        ++SubmitCallCount;
        ++SubmittedCommandBufferCount;
        return ERHIResult::Success;
    }

    ERHIResult WaitIdle() override
    {
        ++WaitIdleCallCount;
        return ERHIResult::Success;
    }

    uint32 SubmitCallCount = 0;
    uint32 WaitIdleCallCount = 0;

private:
    uint32 SubmittedCommandBufferCount = 0;
};

class FTestPipelineLayout final : public IRHIPipelineLayout
{
public:
    [[nodiscard]] const FRHIPipelineLayoutDesc& GetDesc() const noexcept override
    {
        return Desc;
    }

    [[nodiscard]] uint32 GetSetCount() const noexcept override
    {
        return 0;
    }

    [[nodiscard]] const FRHIDescriptorBinding* FindBinding(
        uint32,
        uint32) const noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return State;
    }

    ERHIResult Invalidate() override
    {
        State = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

private:
    FRHIPipelineLayoutDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

FRHIGraphicsPipelineDesc MakeDefaultCompatibleGraphicsPipelineDesc()
{
    FRHIGraphicsPipelineDesc Desc;
    Desc.PipelineLayout = std::make_shared<FTestPipelineLayout>();
    Desc.VertexInput.Stride = 4;
    Desc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}};
    Desc.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
    return Desc;
}

void TestDefaultDeferredSubmit(FRHILabContractTestResult& Result)
{
    const TArray<TSharedPtr<IRHISemaphore>> NoSemaphores;
    const TSharedPtr<IRHICommandBuffer> CommandBuffer =
        std::make_shared<FTestCommandBuffer>();
    FTrackingCommandQueue Queue;

    const ERHIResult NullFenceResult = Queue.SubmitDeferred(
        CommandBuffer, NoSemaphores, NoSemaphores, nullptr);
    Record(Result,
        NullFenceResult == ERHIResult::InvalidState &&
            Queue.SubmitCallCount == 0 &&
            Queue.GetSubmittedCommandBufferCount() == 0 &&
            Queue.WaitIdleCallCount == 0,
        "SubmitDeferred rejects a null mandatory completion fence without forwarding Submit");

    const TSharedPtr<FTestFence> CompletionFence =
        std::make_shared<FTestFence>();
    const ERHIResult UnsupportedResult = Queue.SubmitDeferred(
        CommandBuffer, NoSemaphores, NoSemaphores, CompletionFence);
    Record(Result,
        UnsupportedResult == ERHIResult::Unsupported &&
            Queue.SubmitCallCount == 0 &&
            Queue.GetSubmittedCommandBufferCount() == 0 &&
            Queue.WaitIdleCallCount == 0 &&
            !CompletionFence->IsSignaled() &&
            CompletionFence->GetState() == ERHIFenceState::Unsignaled,
        "base SubmitDeferred returns Unsupported without submitting or completing work");
}

void TestColorWriteMaskContracts(FRHILabContractTestResult& Result)
{
    const FRHIGraphicsPipelineDesc DefaultDesc;
    Record(Result,
        DefaultDesc.Blend.ColorWriteMask == ERHIColorWriteMask::RGBA &&
            IsValidRHIColorWriteMask(DefaultDesc.Blend.ColorWriteMask),
        "FRHIBlendState defaults to a valid RGBA color-write mask");

    const ERHIColorWriteMask AllChannels =
        ERHIColorWriteMask::Red |
        ERHIColorWriteMask::Green |
        ERHIColorWriteMask::Blue |
        ERHIColorWriteMask::Alpha;
    Record(Result,
        AllChannels == ERHIColorWriteMask::RGBA &&
            HasRHIFlag(AllChannels, ERHIColorWriteMask::Red) &&
            HasRHIFlag(AllChannels, ERHIColorWriteMask::Alpha),
        "color-write mask flags compose to RGBA with channel membership");

    FRHIGraphicsPipelineDesc CompatibleDesc =
        MakeDefaultCompatibleGraphicsPipelineDesc();
    Record(Result,
        CompatibleDesc.Blend.ColorWriteMask == ERHIColorWriteMask::RGBA &&
            IsValidRHIGraphicsPipelineState(CompatibleDesc),
        "an existing default graphics pipeline remains compatible with RGBA");

    FRHIGraphicsPipelineDesc InvalidDesc = CompatibleDesc;
    InvalidDesc.Blend.ColorWriteMask =
        static_cast<ERHIColorWriteMask>(1u << 4);
    Record(Result,
        !IsValidRHIColorWriteMask(InvalidDesc.Blend.ColorWriteMask) &&
            !IsValidRHIGraphicsPipelineState(InvalidDesc),
        "graphics pipeline validation rejects unknown color-write mask bits");
}

FRHIPresentationFrame MakeTestPresentationFrame(uint32 ImageIndex = 7)
{
    FRHIPresentationFrame Frame;
    Frame.FrameToken = 11;
    Frame.ModeGeneration = 13;
    Frame.SwapchainImageGeneration = 17;
    Frame.ImageIndex = ImageIndex;
    Frame.Width = 640;
    Frame.Height = 360;
    Frame.Format = ERHIFormat::R8G8B8A8_UNorm;
    Frame.ColorSpace = ERHIPresentationColorSpace::SrgbNonlinear;
    return Frame;
}

void TestCapabilitiesAndPresentationLeaseValues(
    FRHILabContractTestResult& Result)
{
    const FRHIDeviceCapabilities DeviceCapabilities;
    const FRHIPresentationCapabilities PresentationCapabilities;
    Record(Result,
        !DeviceCapabilities.bSupportsDeferredSubmission &&
            !PresentationCapabilities.bSupportsIndependentPresentationCompletion,
        "interactive deferred and presentation-fence capabilities default to unsupported");

    const FRHIPresentationFrame Frame = MakeTestPresentationFrame();
    const TSharedPtr<IRHITexture> Texture = std::make_shared<FTestTexture>();
    FRHIBorrowedAcquiredTarget Target;
    Target.Texture = Texture;
    Target.Frame = Frame;
    Target.FrameSlotIndex = 1;
    Record(Result,
        Target.IsValid() && Target.Matches(Frame) &&
            Target.Frame.ImageIndex == 7 && Target.FrameSlotIndex == 1,
        "borrowed acquired target validates its generation and independent image/slot identities");

    FRHIBorrowedAcquiredTarget StaleTarget = Target;
    ++StaleTarget.Frame.ModeGeneration;
    Record(Result,
        !Target.Matches(StaleTarget.Frame) && !Target.Matches(StaleTarget),
        "borrowed target rejects stale mode generations before use");

    FRHIBorrowedAcquiredTarget UnrelatedTextureTarget = Target;
    UnrelatedTextureTarget.Texture = std::make_shared<FTestTexture>();
    Record(Result,
        !Target.Matches(UnrelatedTextureTarget),
        "borrowed target rejects an unrelated texture for the same frame generation");

    FRHIBorrowedAcquiredTarget WrongExtentTarget = Target;
    WrongExtentTarget.Texture = std::make_shared<FTestTexture>(320, 180);
    Record(Result,
        !WrongExtentTarget.IsValid(),
        "borrowed target rejects a texture with a mismatched drawable extent");

    FRHIBorrowedAcquiredTarget InvalidatedTarget = Target;
    const TSharedPtr<FTestTexture> InvalidatedTexture =
        std::make_shared<FTestTexture>();
    (void)InvalidatedTexture->Invalidate();
    InvalidatedTarget.Texture = InvalidatedTexture;
    Record(Result,
        !InvalidatedTarget.IsValid(),
        "borrowed target rejects an invalidated texture generation");

    FRHIBorrowedAcquiredTarget TooManyImages = Target;
    TooManyImages.Frame.ImageIndex = MaxRHIPresentationImageLeases;
    Record(Result,
        !TooManyImages.IsValid(),
        "borrowed target rejects an image index outside the bounded lease set");

    const TSharedPtr<FTestFence> RenderFence = std::make_shared<FTestFence>();
    FRHIRenderLease RenderLease;
    RenderLease.Frame = Frame;
    RenderLease.FrameSlotIndex = Target.FrameSlotIndex;
    RenderLease.CompletionFence = RenderFence;
    Record(Result,
        RenderLease.IsValid() && RenderLease.Matches(Target) &&
            !RenderFence->IsSignaled(),
        "render lease owns completion independently from presentation release");

    const TSharedPtr<FTestSemaphore> RenderFinished =
        std::make_shared<FTestSemaphore>();
    const TSharedPtr<FTestFence> PresentationFence =
        std::make_shared<FTestFence>();
    FRHIPresentationLease PresentationLease;
    PresentationLease.Frame = Frame;
    PresentationLease.RenderFinishedSemaphore = RenderFinished;
    PresentationLease.PresentationCompletionFence = PresentationFence;
    Record(Result,
        PresentationLease.IsValid() && PresentationLease.Matches(Target) &&
            !PresentationFence->IsSignaled(),
        "presentation lease retains image-indexed ownership independently of render slot completion");

    FRHIPresentationLease CallbackPresentationLease;
    CallbackPresentationLease.Frame = Frame;
    CallbackPresentationLease.PresentationCompletionFence =
        std::make_shared<FTestFence>();
    Record(Result,
        CallbackPresentationLease.IsValid(),
        "presentation lease remains backend-neutral when completion is callback-owned");

    FRHIRenderLease WrongSlotLease = RenderLease;
    WrongSlotLease.FrameSlotIndex = 0;
    Record(Result,
        !WrongSlotLease.Matches(Target),
        "lease matching includes frame slot without conflating it with image index");
}

} // namespace

int RunRHILabContractTests()
{
    FRHILabContractTestResult Result;
    std::cout << "[INFO] Running RHI interactive-lab contract tests\n";
    TestDefaultDeferredSubmit(Result);
    TestColorWriteMaskContracts(Result);
    TestCapabilitiesAndPresentationLeaseValues(Result);
    std::cout << "[INFO] RHI interactive-lab contract tests passed="
              << Result.Passed << " failed=" << Result.Failed << '\n';
    return Result.Failed == 0 ? 0 : 1;
}
