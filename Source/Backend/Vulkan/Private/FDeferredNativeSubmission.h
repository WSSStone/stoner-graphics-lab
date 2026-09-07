#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>

namespace Stoner::RHI
{
class IRHIBuffer;
}

namespace Stoner::Backend::Vulkan
{

class FVulkanCommandBuffer;
class FVulkanBuffer;
class FVulkanFence;
class FVulkanNativeContext;

// Owns every native object whose lifetime extends beyond SubmitDeferred.  The
// context records commands into this object, while the queue/fence only
// observe it through backend-neutral result and status methods.
class FDeferredNativeSubmission final
{
public:
    FDeferredNativeSubmission(
        VkDevice InDevice,
        VkQueue InQueue,
        Stoner::Core::uint32 InQueueFamily,
        FVulkanNativeContext* InContext,
        const Stoner::Core::TSharedPtr<FVulkanCommandBuffer>& InCommandBuffer,
        const Stoner::Core::TSharedPtr<FVulkanFence>& InCompletionFence,
        Stoner::Core::uint64 InSubmissionId) noexcept;
    ~FDeferredNativeSubmission();

    FDeferredNativeSubmission(const FDeferredNativeSubmission&) = delete;
    FDeferredNativeSubmission& operator=(const FDeferredNativeSubmission&) = delete;

    [[nodiscard]] Stoner::RHI::ERHIResult Initialize() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult Submit() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult Poll(
        Stoner::Core::uint64 TimeoutMicroseconds) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult WaitForCompletion() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult GetResult() const noexcept;
    // Native fence completion is kept separate from public fence publication:
    // the context calls this only after readback/layout finalization succeeds.
    void PublishCompletion() noexcept;
    void MarkCompletionProcessingFailure(
        Stoner::RHI::ERHIResult Failure) noexcept;
    [[nodiscard]] bool IsComplete() const noexcept;
    [[nodiscard]] bool IsCompletionProven() const noexcept;
    [[nodiscard]] bool IsSubmitted() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetSubmissionId() const noexcept;
    [[nodiscard]] VkCommandBuffer GetCommandBuffer() const noexcept;
    [[nodiscard]] VkCommandPool GetCommandPool() const noexcept;
    [[nodiscard]] VkFence GetNativeFence() const noexcept;

    // Test-only observation gate. Native work is submitted normally and its
    // real Vulkan fence is queried after SignalHostCompletion releases the
    // gate; no extra Vulkan synchronization feature is required.
    void ConfigureHostCompletionDelay(bool bEnabled) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult SignalHostCompletion() noexcept;
    void ReleaseNativeResources() noexcept;
    // Used only after the owning context has completed device-idle teardown.
    // A query error cannot otherwise authorize destruction of submitted work.
    void ForceReleaseNativeResourcesAfterDeviceIdle() noexcept;
    // Device-loss teardown abandons native handles without claiming that the
    // submitted work completed. It still releases backend ownership leases.
    void AbandonNativeResourcesAfterDeviceLoss() noexcept;

    [[nodiscard]] std::vector<VkImageView>& GetImageViews() noexcept;
    [[nodiscard]] std::vector<VkSampler>& GetSamplers() noexcept;
    [[nodiscard]] std::vector<VkDescriptorPool>& GetDescriptorPools() noexcept;
    [[nodiscard]] std::vector<VkRenderPass>& GetRenderPasses() noexcept;
    [[nodiscard]] std::vector<VkFramebuffer>& GetFramebuffers() noexcept;
    [[nodiscard]] std::unordered_map<Stoner::Core::uint64,
        std::vector<VkImageLayout>>& GetTextureLayouts() noexcept;
    [[nodiscard]] const std::unordered_map<Stoner::Core::uint64,
        std::vector<VkImageLayout>>& GetTextureLayouts() const noexcept;

    [[nodiscard]] const Stoner::Core::TSharedPtr<FVulkanCommandBuffer>&
    GetRetainedCommandBuffer() const noexcept;
    [[nodiscard]] bool RetainBuffer(
        const Stoner::Core::TSharedPtr<FVulkanBuffer>& Buffer) noexcept;
    [[nodiscard]] bool RetainPipeline(
        Stoner::Core::uint64 Token) noexcept;
    [[nodiscard]] bool RetainTexture(
        Stoner::Core::uint64 Token,
        std::vector<VkImageLayout>& OutLayouts) noexcept;
    [[nodiscard]] bool IsDeferredSubmission() const noexcept
    {
        return CompletionFence_ != nullptr;
    }
    [[nodiscard]] bool MarkReadbackBuffer(
        const Stoner::RHI::IRHIBuffer* Buffer) noexcept;
    [[nodiscard]] const std::vector<Stoner::Core::TSharedPtr<FVulkanBuffer>>&
    GetRetainedBuffers() const noexcept;
    [[nodiscard]] const std::vector<Stoner::Core::TSharedPtr<FVulkanBuffer>>&
    GetReadbackBuffers() const noexcept;

private:
    [[nodiscard]] Stoner::RHI::ERHIResult Complete(
        bool bSucceeded) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult MapNativeResult(
        VkResult Result) const noexcept;

    VkDevice Device_ = VK_NULL_HANDLE;
    VkQueue Queue_ = VK_NULL_HANDLE;
    Stoner::Core::uint32 QueueFamily_ = 0;
    FVulkanNativeContext* Context_ = nullptr;
    Stoner::Core::TSharedPtr<FVulkanCommandBuffer> CommandBufferOwner_;
    Stoner::Core::TSharedPtr<FVulkanFence> CompletionFence_;
    Stoner::Core::uint64 SubmissionId_ = 0;
    VkCommandPool CommandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer CommandBuffer_ = VK_NULL_HANDLE;
    VkFence Fence_ = VK_NULL_HANDLE;
    bool bSubmitted_ = false;
    bool bComplete_ = false;
    bool bCompletionProven_ = false;
    bool bHostCompletionDelay_ = false;
    bool bHostCompletionReleased_ = true;
    Stoner::RHI::ERHIResult Result_ = Stoner::RHI::ERHIResult::InvalidState;

    std::vector<VkImageView> ImageViews_;
    std::vector<VkSampler> Samplers_;
    std::vector<VkDescriptorPool> DescriptorPools_;
    std::vector<VkRenderPass> RenderPasses_;
    std::vector<VkFramebuffer> Framebuffers_;
    std::unordered_map<Stoner::Core::uint64, std::vector<VkImageLayout>>
        TextureLayouts_;
    std::vector<Stoner::Core::TSharedPtr<FVulkanBuffer>> RetainedBuffers_;
    std::vector<Stoner::Core::TSharedPtr<FVulkanBuffer>> ReadbackBuffers_;
    std::vector<Stoner::Core::uint64> RetainedPipelineTokens_;
    std::vector<Stoner::Core::uint64> RetainedTextureTokens_;
};

} // namespace Stoner::Backend::Vulkan

#else

namespace Stoner::Backend::Vulkan
{
class FDeferredNativeSubmission final {};
} // namespace Stoner::Backend::Vulkan

#endif
