#include "FDeferredNativeSubmission.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanBuffer.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#include <algorithm>
#include <limits>

namespace Stoner::Backend::Vulkan
{

FDeferredNativeSubmission::FDeferredNativeSubmission(
    VkDevice InDevice,
    VkQueue InQueue,
    Stoner::Core::uint32 InQueueFamily,
    FVulkanNativeContext* InContext,
    const Stoner::Core::TSharedPtr<FVulkanCommandBuffer>& InCommandBuffer,
    const Stoner::Core::TSharedPtr<FVulkanFence>& InCompletionFence,
    Stoner::Core::uint64 InSubmissionId) noexcept
    : Device_(InDevice)
    , Queue_(InQueue)
    , QueueFamily_(InQueueFamily)
    , Context_(InContext)
    , CommandBufferOwner_(InCommandBuffer)
    , CompletionFence_(InCompletionFence)
    , SubmissionId_(InSubmissionId)
{
}

FDeferredNativeSubmission::~FDeferredNativeSubmission()
{
    // A submitted record is only destroyed by the context after its fence has
    // completed.  Keeping this guard makes accidental early destruction fail
    // closed instead of freeing command resources while the queue uses them.
    if (!bSubmitted_ || bComplete_)
        ReleaseNativeResources();
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::Initialize() noexcept
{
    using namespace Stoner::RHI;
    if (Device_ == VK_NULL_HANDLE || Queue_ == VK_NULL_HANDLE ||
        SubmissionId_ == 0 || CommandBuffer_ != VK_NULL_HANDLE ||
        Fence_ != VK_NULL_HANDLE)
        return ERHIResult::InvalidState;

    VkCommandPoolCreateInfo PoolInfo{};
    PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    PoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    PoolInfo.queueFamilyIndex = QueueFamily_;
    VkResult NativeResult = vkCreateCommandPool(
        Device_, &PoolInfo, nullptr, &CommandPool_);
    if (NativeResult != VK_SUCCESS)
        return MapNativeResult(NativeResult);

    VkCommandBufferAllocateInfo AllocateInfo{};
    AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocateInfo.commandPool = CommandPool_;
    AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocateInfo.commandBufferCount = 1;
    NativeResult = vkAllocateCommandBuffers(
        Device_, &AllocateInfo, &CommandBuffer_);
    if (NativeResult != VK_SUCCESS)
    {
        ReleaseNativeResources();
        return MapNativeResult(NativeResult);
    }

    VkFenceCreateInfo FenceInfo{};
    FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    NativeResult = vkCreateFence(Device_, &FenceInfo, nullptr, &Fence_);
    if (NativeResult != VK_SUCCESS)
    {
        ReleaseNativeResources();
        return MapNativeResult(NativeResult);
    }

    Result_ = ERHIResult::Success;
    return Result_;
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::Submit() noexcept
{
    return Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::Submit(
    VkSemaphore AcquireWait,
    VkSemaphore RenderFinishedSignal) noexcept
{
    using namespace Stoner::RHI;
    if (Result_ != ERHIResult::Success || CommandBuffer_ == VK_NULL_HANDLE ||
        Fence_ == VK_NULL_HANDLE || bSubmitted_ || bComplete_)
        return ERHIResult::InvalidState;

    VkSubmitInfo SubmitInfo{};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // This recorder can transition/copy the acquired image before attachment
    // output. Keep acquisition ahead of every command until the caller can
    // provide a narrower scope matched to its first image access/barrier.
    VkPipelineStageFlags WaitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    SubmitInfo.waitSemaphoreCount = AcquireWait != VK_NULL_HANDLE ? 1 : 0;
    SubmitInfo.pWaitSemaphores =
        AcquireWait != VK_NULL_HANDLE ? &AcquireWait : nullptr;
    SubmitInfo.pWaitDstStageMask =
        AcquireWait != VK_NULL_HANDLE ? &WaitStageMask : nullptr;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CommandBuffer_;
    SubmitInfo.signalSemaphoreCount =
        RenderFinishedSignal != VK_NULL_HANDLE ? 1 : 0;
    SubmitInfo.pSignalSemaphores =
        RenderFinishedSignal != VK_NULL_HANDLE ? &RenderFinishedSignal : nullptr;
    const VkResult NativeResult = vkQueueSubmit(
        Queue_, 1, &SubmitInfo, Fence_);
    if (NativeResult != VK_SUCCESS)
    {
        Result_ = MapNativeResult(NativeResult);
        return Result_;
    }
    bSubmitted_ = true;
    if (Context_) Context_->RecordNativeRenderOperation(false);
    return ERHIResult::Success;
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::Poll(
    Stoner::Core::uint64 TimeoutMicroseconds) noexcept
{
    using namespace Stoner::RHI;
    if (!bSubmitted_ || Fence_ == VK_NULL_HANDLE)
        return Result_ == ERHIResult::Success && bComplete_
            ? Result_ : ERHIResult::InvalidState;
    if (bComplete_)
        return Result_;

    // This gate intentionally withholds host observation after a real queue
    // submit. It does not alter command execution or fence signaling, so the
    // release path below still proves completion using the native fence.
    if (bHostCompletionDelay_ && !bHostCompletionReleased_)
        return TimeoutMicroseconds == 0
            ? ERHIResult::NotReady
            : ERHIResult::Timeout;

    VkResult NativeResult = vkGetFenceStatus(Device_, Fence_);
    if (NativeResult == VK_NOT_READY && TimeoutMicroseconds != 0)
    {
        constexpr Stoner::Core::uint64 NanosecondsPerMicrosecond = 1000;
        // UINT64_MAX denotes an unbounded Vulkan wait. Keep every finite RHI
        // wait finite, even when the microsecond value would overflow ns.
        const Stoner::Core::uint64 MaxFiniteTimeout =
            std::numeric_limits<Stoner::Core::uint64>::max() - 1;
        const Stoner::Core::uint64 TimeoutNanoseconds =
            TimeoutMicroseconds > MaxFiniteTimeout / NanosecondsPerMicrosecond
            ? MaxFiniteTimeout
            : TimeoutMicroseconds * NanosecondsPerMicrosecond;
        if (Context_) Context_->RecordNativeSubmissionWait(!GetReadbackBuffers().empty());
        NativeResult = vkWaitForFences(
            Device_, 1, &Fence_, VK_TRUE, TimeoutNanoseconds);
    }
    if (NativeResult == VK_NOT_READY)
        return ERHIResult::NotReady;
    if (NativeResult == VK_TIMEOUT)
        return ERHIResult::Timeout;
    if (NativeResult != VK_SUCCESS)
    {
        Result_ = MapNativeResult(NativeResult);
        return Complete(false);
    }
    return Complete(true);
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::WaitForCompletion() noexcept
{
    using namespace Stoner::RHI;
    if (!bSubmitted_ || Fence_ == VK_NULL_HANDLE)
        return Result_ == ERHIResult::Success && bComplete_
            ? Result_ : ERHIResult::InvalidState;
    if (bComplete_)
        return Result_;
    if (Context_) Context_->RecordNativeSubmissionWait(!GetReadbackBuffers().empty());
    const VkResult NativeResult = vkWaitForFences(
        Device_, 1, &Fence_, VK_TRUE, std::numeric_limits<uint64_t>::max());
    if (NativeResult != VK_SUCCESS)
    {
        Result_ = MapNativeResult(NativeResult);
        return Complete(false);
    }
    return Complete(true);
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::GetResult() const noexcept
{
    return Result_;
}

void FDeferredNativeSubmission::PublishCompletion() noexcept
{
    if (bComplete_ && bCompletionProven_ &&
        Result_ == Stoner::RHI::ERHIResult::Success && CompletionFence_)
    {
        CompletionFence_->CompleteNativeSubmission(true);
    }
}

void FDeferredNativeSubmission::MarkCompletionProcessingFailure(
    Stoner::RHI::ERHIResult Failure) noexcept
{
    if (Failure == Stoner::RHI::ERHIResult::Success)
        Failure = Stoner::RHI::ERHIResult::Failed;
    Result_ = Failure;
    if (CompletionFence_)
        CompletionFence_->CompleteNativeSubmission(false);
}

bool FDeferredNativeSubmission::IsComplete() const noexcept
{
    return bComplete_;
}

bool FDeferredNativeSubmission::IsSubmitted() const noexcept
{
    return bSubmitted_;
}

bool FDeferredNativeSubmission::IsCompletionProven() const noexcept
{
    return bCompletionProven_;
}

Stoner::Core::uint64 FDeferredNativeSubmission::GetSubmissionId() const noexcept
{
    return SubmissionId_;
}

VkCommandBuffer FDeferredNativeSubmission::GetCommandBuffer() const noexcept
{
    return CommandBuffer_;
}

VkCommandPool FDeferredNativeSubmission::GetCommandPool() const noexcept
{
    return CommandPool_;
}

VkFence FDeferredNativeSubmission::GetNativeFence() const noexcept
{
    return Fence_;
}

void FDeferredNativeSubmission::ConfigureHostCompletionDelay(
    bool bEnabled) noexcept
{
    if (!bSubmitted_ && !bComplete_)
    {
        bHostCompletionDelay_ = bEnabled;
        bHostCompletionReleased_ = !bEnabled;
    }
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::SignalHostCompletion() noexcept
{
    if (!bHostCompletionDelay_ || !bSubmitted_ || bComplete_)
        return Stoner::RHI::ERHIResult::InvalidState;
    bHostCompletionReleased_ = true;
    return Stoner::RHI::ERHIResult::Success;
}

void FDeferredNativeSubmission::ReleaseNativeResources() noexcept
{
    if (bSubmitted_ && !bCompletionProven_)
        return;
    if (Device_ == VK_NULL_HANDLE)
        return;
    for (VkFramebuffer Value : Framebuffers_)
        if (Value != VK_NULL_HANDLE) vkDestroyFramebuffer(Device_, Value, nullptr);
    for (VkRenderPass Value : RenderPasses_)
        if (Value != VK_NULL_HANDLE) vkDestroyRenderPass(Device_, Value, nullptr);
    for (VkDescriptorPool Value : DescriptorPools_)
        if (Value != VK_NULL_HANDLE) vkDestroyDescriptorPool(Device_, Value, nullptr);
    for (VkSampler Value : Samplers_)
        if (Value != VK_NULL_HANDLE) vkDestroySampler(Device_, Value, nullptr);
    for (VkImageView Value : ImageViews_)
        if (Value != VK_NULL_HANDLE) vkDestroyImageView(Device_, Value, nullptr);
    if (Fence_ != VK_NULL_HANDLE)
        vkDestroyFence(Device_, Fence_, nullptr);
    if (CommandPool_ != VK_NULL_HANDLE)
        vkDestroyCommandPool(Device_, CommandPool_, nullptr);
    ImageViews_.clear();
    Samplers_.clear();
    DescriptorPools_.clear();
    RenderPasses_.clear();
    Framebuffers_.clear();
    TextureLayouts_.clear();
    ReadbackBuffers_.clear();
    if (Context_ != nullptr)
    {
        for (const Stoner::Core::uint64 Token : RetainedPipelineTokens_)
            Context_->ReleaseDeferredPipelineUse(Token);
        for (const Stoner::Core::uint64 Token : RetainedTextureTokens_)
            Context_->ReleaseDeferredTextureUse(Token);
    }
    RetainedPipelineTokens_.clear();
    RetainedTextureTokens_.clear();
    for (const auto& Buffer : RetainedBuffers_)
        if (Buffer) Buffer->ReleaseNativeUse();
    RetainedBuffers_.clear();
    Fence_ = VK_NULL_HANDLE;
    CommandBuffer_ = VK_NULL_HANDLE;
    CommandPool_ = VK_NULL_HANDLE;
    Device_ = VK_NULL_HANDLE;
    Queue_ = VK_NULL_HANDLE;
    Context_ = nullptr;
    CommandBufferOwner_.reset();
    CompletionFence_.reset();
}

void FDeferredNativeSubmission::ForceReleaseNativeResourcesAfterDeviceIdle() noexcept
{
    bCompletionProven_ = true;
    ReleaseNativeResources();
}

void FDeferredNativeSubmission::AbandonNativeResourcesAfterDeviceLoss() noexcept
{
    if (Context_ != nullptr)
    {
        for (const Stoner::Core::uint64 Token : RetainedPipelineTokens_)
            Context_->ReleaseDeferredPipelineUse(Token);
        for (const Stoner::Core::uint64 Token : RetainedTextureTokens_)
            Context_->ReleaseDeferredTextureUse(Token);
    }
    RetainedPipelineTokens_.clear();
    RetainedTextureTokens_.clear();
    for (const auto& Buffer : RetainedBuffers_)
        if (Buffer) Buffer->ReleaseNativeUse();
    RetainedBuffers_.clear();
    ImageViews_.clear();
    Samplers_.clear();
    DescriptorPools_.clear();
    RenderPasses_.clear();
    Framebuffers_.clear();
    TextureLayouts_.clear();
    ReadbackBuffers_.clear();
    // Do not destroy handles after device loss and do not set completion
    // proven. The owning context destroys the device immediately afterwards.
    Fence_ = VK_NULL_HANDLE;
    CommandBuffer_ = VK_NULL_HANDLE;
    CommandPool_ = VK_NULL_HANDLE;
    Device_ = VK_NULL_HANDLE;
    Queue_ = VK_NULL_HANDLE;
    Context_ = nullptr;
    CommandBufferOwner_.reset();
    CompletionFence_.reset();
    bSubmitted_ = false;
}

std::vector<VkImageView>& FDeferredNativeSubmission::GetImageViews() noexcept
{
    return ImageViews_;
}

std::vector<VkSampler>& FDeferredNativeSubmission::GetSamplers() noexcept
{
    return Samplers_;
}

std::vector<VkDescriptorPool>&
FDeferredNativeSubmission::GetDescriptorPools() noexcept
{
    return DescriptorPools_;
}

std::vector<VkRenderPass>& FDeferredNativeSubmission::GetRenderPasses() noexcept
{
    return RenderPasses_;
}

std::vector<VkFramebuffer>& FDeferredNativeSubmission::GetFramebuffers() noexcept
{
    return Framebuffers_;
}

std::unordered_map<Stoner::Core::uint64, std::vector<VkImageLayout>>&
FDeferredNativeSubmission::GetTextureLayouts() noexcept
{
    return TextureLayouts_;
}

const std::unordered_map<Stoner::Core::uint64, std::vector<VkImageLayout>>&
FDeferredNativeSubmission::GetTextureLayouts() const noexcept
{
    return TextureLayouts_;
}

const Stoner::Core::TSharedPtr<FVulkanCommandBuffer>&
FDeferredNativeSubmission::GetRetainedCommandBuffer() const noexcept
{
    return CommandBufferOwner_;
}

bool FDeferredNativeSubmission::RetainBuffer(
    const Stoner::Core::TSharedPtr<FVulkanBuffer>& Buffer) noexcept
{
    if (!Buffer)
        return false;
    if (std::find(RetainedBuffers_.begin(), RetainedBuffers_.end(), Buffer) ==
        RetainedBuffers_.end())
    {
        if (!Buffer->AcquireNativeUse())
            return false;
        try
        {
            RetainedBuffers_.push_back(Buffer);
        }
        catch (...)
        {
            // The encoder handles allocation failures around command
            // recording. This helper remains noexcept for teardown paths.
            Buffer->ReleaseNativeUse();
            return false;
        }
    }
    return true;
}

bool FDeferredNativeSubmission::RetainPipeline(
    Stoner::Core::uint64 Token) noexcept
{
    if (Token == 0)
        return false;
    if (std::find(RetainedPipelineTokens_.begin(),
            RetainedPipelineTokens_.end(), Token) !=
        RetainedPipelineTokens_.end())
        return true;
    if (Context_ == nullptr ||
        !Context_->AcquireDeferredPipelineUse(Token))
        return false;
    try
    {
        RetainedPipelineTokens_.push_back(Token);
    }
    catch (...)
    {
        Context_->ReleaseDeferredPipelineUse(Token);
        return false;
    }
    return true;
}

bool FDeferredNativeSubmission::RetainTexture(
    Stoner::Core::uint64 Token,
    std::vector<VkImageLayout>& OutLayouts) noexcept
{
    if (Token == 0)
        return false;
    if (std::find(RetainedTextureTokens_.begin(),
            RetainedTextureTokens_.end(), Token) !=
        RetainedTextureTokens_.end())
        return true;
    Stoner::Core::TArray<Stoner::Core::uint32> LayoutValues;
    if (Context_ == nullptr ||
        !Context_->AcquireDeferredTextureUse(Token, LayoutValues))
        return false;
    try
    {
        OutLayouts.clear();
        OutLayouts.reserve(LayoutValues.size());
        for (const Stoner::Core::uint32 Value : LayoutValues)
            OutLayouts.push_back(static_cast<VkImageLayout>(Value));
        RetainedTextureTokens_.push_back(Token);
    }
    catch (...)
    {
        OutLayouts.clear();
        Context_->ReleaseDeferredTextureUse(Token);
        return false;
    }
    return true;
}

bool FDeferredNativeSubmission::MarkReadbackBuffer(
    const Stoner::RHI::IRHIBuffer* Buffer) noexcept
{
    if (Buffer == nullptr)
        return false;
    for (const auto& Candidate : RetainedBuffers_)
    {
        if (Candidate.get() == Buffer &&
            std::find(ReadbackBuffers_.begin(), ReadbackBuffers_.end(), Candidate) ==
                ReadbackBuffers_.end())
        {
            try
            {
                ReadbackBuffers_.push_back(Candidate);
            }
            catch (...)
            {
                return false;
            }
            return true;
        }
        if (Candidate.get() == Buffer)
            return true;
    }
    return false;
}

const std::vector<Stoner::Core::TSharedPtr<FVulkanBuffer>>&
FDeferredNativeSubmission::GetRetainedBuffers() const noexcept
{
    return RetainedBuffers_;
}

const std::vector<Stoner::Core::TSharedPtr<FVulkanBuffer>>&
FDeferredNativeSubmission::GetReadbackBuffers() const noexcept
{
    return ReadbackBuffers_;
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::Complete(
    bool bSucceeded) noexcept
{
    if (bComplete_)
        return Result_;
    if (bSucceeded && IsDeferredSubmission() && CommandBufferOwner_ &&
        CommandBufferOwner_->MarkCompletedOrResettable() !=
            Stoner::RHI::ERHIResult::Success)
    {
        bSucceeded = false;
    }
    bComplete_ = true;
    bCompletionProven_ = bSucceeded;
    if (bSubmitted_ && bSucceeded && Context_) Context_->RecordNativeRenderOperation(true);
    if (bSucceeded)
    {
        Result_ = Stoner::RHI::ERHIResult::Success;
    }
    else if (Result_ == Stoner::RHI::ERHIResult::Success)
    {
        // Preserve a concrete native wait/status error selected by the
        // caller; only an otherwise unclassified completion failure maps to
        // the generic RHI failure result.
        Result_ = Stoner::RHI::ERHIResult::Failed;
    }
    if (!bSucceeded && CompletionFence_)
        CompletionFence_->CompleteNativeSubmission(false);
    return Result_;
}

Stoner::RHI::ERHIResult FDeferredNativeSubmission::MapNativeResult(
    VkResult Result) const noexcept
{
    using namespace Stoner::RHI;
    if (Result == VK_SUCCESS) return ERHIResult::Success;
    if (Result == VK_TIMEOUT) return ERHIResult::Timeout;
    if (Result == VK_NOT_READY) return ERHIResult::NotReady;
    if (Result == VK_ERROR_OUT_OF_HOST_MEMORY ||
        Result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
        return ERHIResult::Unavailable;
    return ERHIResult::Failed;
}

} // namespace Stoner::Backend::Vulkan

#endif
