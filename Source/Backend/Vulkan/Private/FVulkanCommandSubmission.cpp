#include "VulkanRHI/FVulkanCommandSubmission.h"

#include "VulkanRHI/FVulkanCommandBuffer.h"

namespace Stoner::Backend::Vulkan
{

FVulkanCommandSubmission::FVulkanCommandSubmission(Stoner::Core::TSharedPtr<FVulkanCommandBuffer> InCommandBuffer, EVulkanSubmissionMode InMode, FVulkanCompletionInjectionConfig InInjection) noexcept
    : CommandBuffer(std::move(InCommandBuffer))
    , Mode(InMode)
    , Injection(InInjection)
{
}

EVulkanSubmissionMode FVulkanCommandSubmission::GetMode() const noexcept { return Mode; }
EVulkanCompletionState FVulkanCommandSubmission::GetCompletionState() const noexcept { return CompletionState; }
Stoner::Core::TSharedPtr<FVulkanCommandBuffer> FVulkanCommandSubmission::GetCommandBuffer() const noexcept { return CommandBuffer; }
bool FVulkanCommandSubmission::IsValid() const noexcept { return bValid; }

Stoner::RHI::ERHIResult FVulkanCommandSubmission::ObserveCompletion(Stoner::Core::uint64 TimeoutMicroseconds) noexcept
{
    if (!bValid || !CommandBuffer)
    {
        CompletionState = EVulkanCompletionState::Invalidated;
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Injection.bForceTimeout)
    {
        CompletionState = EVulkanCompletionState::Timeout;
        return Stoner::RHI::ERHIResult::Timeout;
    }
    if (TimeoutMicroseconds > 0)
    {
        // Fallback submissions complete immediately; a non-zero timeout
        // still resolves to Success because there is no real GPU work.
        CompletionState = EVulkanCompletionState::Completed;
        return Stoner::RHI::ERHIResult::Success;
    }
    if (Injection.bForceNotReady)
    {
        CompletionState = EVulkanCompletionState::NotReady;
        return Stoner::RHI::ERHIResult::NotReady;
    }
    CompletionState = EVulkanCompletionState::Completed;
    return CommandBuffer->MarkCompletedOrResettable();
}

Stoner::RHI::ERHIResult FVulkanCommandSubmission::CompleteForWaitIdle() noexcept
{
    if (!bValid || !CommandBuffer)
    {
        CompletionState = EVulkanCompletionState::Invalidated;
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    CompletionState = EVulkanCompletionState::Completed;
    return CommandBuffer->MarkCompletedOrResettable();
}

void FVulkanCommandSubmission::Invalidate() noexcept
{
    bValid = false;
    CompletionState = EVulkanCompletionState::Invalidated;
}

} // namespace Stoner::Backend::Vulkan
