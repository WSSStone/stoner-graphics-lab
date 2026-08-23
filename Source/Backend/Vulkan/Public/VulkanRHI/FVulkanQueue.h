#pragma once

#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanCommandSubmission.h"

namespace Stoner::Backend::Vulkan
{

struct FVulkanDiagnostics;
struct FVulkanDeviceOwnerState;
class FVulkanNativeContext;

class FVulkanQueue final : public Stoner::RHI::IRHICommandQueue
{
public:
    ~FVulkanQueue() override = default;

    [[nodiscard]] Stoner::RHI::ERHIQueueType GetQueueType() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetSubmittedCommandBufferCount() const noexcept override;
    [[nodiscard]] bool IsValid() const noexcept;

    Stoner::RHI::ERHIResult Submit(
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& CommandBuffer,
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& WaitSemaphores = {},
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& SignalSemaphores = {},
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>& Fence = nullptr) override;
    Stoner::RHI::ERHIResult WaitIdle() override;
    Stoner::RHI::ERHIResult ObserveLastSubmissionCompletion(Stoner::Core::uint64 TimeoutMicroseconds = 0) noexcept;
    void ConfigureCompletionInjection(FVulkanCompletionInjectionConfig InInjection) noexcept;

private:
    friend class FVulkanDevice;

    FVulkanQueue(
        Stoner::RHI::ERHIQueueType InQueueType,
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner,
        FVulkanDiagnostics* InDiagnostics,
        FVulkanCompletionInjectionConfig InInjection,
        Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext) noexcept;
    void Invalidate() noexcept;

    Stoner::RHI::ERHIQueueType QueueType;
    Stoner::Core::uint32 SubmittedCommandBufferCount = 0;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    FVulkanDiagnostics* Diagnostics = nullptr;
    FVulkanCompletionInjectionConfig CompletionInjection;
    Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeContext;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanCommandSubmission>> Submissions;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
