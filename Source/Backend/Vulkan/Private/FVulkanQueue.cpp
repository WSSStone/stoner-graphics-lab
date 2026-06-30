#include "VulkanRHI/FVulkanQueue.h"

#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanDiagnostics.h"

namespace Stoner::Backend::Vulkan
{

FVulkanQueue::FVulkanQueue(Stoner::RHI::ERHIQueueType InQueueType, FVulkanDiagnostics* InDiagnostics, FVulkanCompletionInjectionConfig InInjection) noexcept
    : QueueType(InQueueType)
    , Diagnostics(InDiagnostics)
    , CompletionInjection(InInjection)
{
}

Stoner::RHI::ERHIQueueType FVulkanQueue::GetQueueType() const noexcept
{
    return QueueType;
}

Stoner::Core::uint32 FVulkanQueue::GetSubmittedCommandBufferCount() const noexcept
{
    return SubmittedCommandBufferCount;
}

bool FVulkanQueue::IsValid() const noexcept
{
    return bValid;
}

Stoner::RHI::ERHIResult FVulkanQueue::Submit(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& CommandBuffer,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& WaitSemaphores,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& SignalSemaphores,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>& Fence)
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!CommandBuffer || CommandBuffer->GetState() != Stoner::RHI::ERHICommandBufferState::Completed || CommandBuffer->GetRecordedCommandCount() == 0)
    {
        if (Diagnostics)
        {
            MarkSubmission(*Diagnostics, "submission rejected by missing or non-executable command buffer");
        }
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (CommandBuffer->GetCompatibleQueueType() != QueueType)
    {
        if (Diagnostics)
        {
            MarkSubmission(*Diagnostics, "submission rejected by incompatible queue");
        }
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    for (const auto& Semaphore : WaitSemaphores)
    {
        if (!Semaphore)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        const Stoner::RHI::ERHIResult ConsumeResult = Semaphore->Consume();
        if (ConsumeResult != Stoner::RHI::ERHIResult::Success)
        {
            return ConsumeResult;
        }
    }

    auto VulkanCommandBuffer = std::dynamic_pointer_cast<FVulkanCommandBuffer>(CommandBuffer);
    if (!VulkanCommandBuffer || VulkanCommandBuffer->MarkSubmitted() != Stoner::RHI::ERHIResult::Success)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    auto Submission = Stoner::Core::MakeShared<FVulkanCommandSubmission>(VulkanCommandBuffer, EVulkanSubmissionMode::DeterministicFallback, CompletionInjection);
    Submissions.push_back(Submission);
    ++SubmittedCommandBufferCount;

    for (const auto& Semaphore : SignalSemaphores)
    {
        if (!Semaphore)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        const Stoner::RHI::ERHIResult SignalResult = Semaphore->Signal();
        if (SignalResult != Stoner::RHI::ERHIResult::Success)
        {
            return SignalResult;
        }
    }
    if (Fence)
    {
        const Stoner::RHI::ERHIResult FenceResult = Fence->Signal();
        if (FenceResult != Stoner::RHI::ERHIResult::Success)
        {
            return FenceResult;
        }
    }

    if (Diagnostics)
    {
        MarkSubmission(*Diagnostics, "deterministic fallback submission recorded; no real GPU execution occurred");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanQueue::WaitIdle()
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    for (const auto& Submission : Submissions)
    {
        if (Submission && Submission->GetCompletionState() == EVulkanCompletionState::Pending)
        {
            const Stoner::RHI::ERHIResult Result = Submission->CompleteForWaitIdle();
            if (Result != Stoner::RHI::ERHIResult::Success)
            {
                return Result;
            }
        }
    }
    if (Diagnostics)
    {
        MarkCompletion(*Diagnostics, "queue wait idle completed fallback submissions");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanQueue::ObserveLastSubmissionCompletion(Stoner::Core::uint64 TimeoutMicroseconds) noexcept
{
    if (!bValid || Submissions.empty() || !Submissions.back())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const Stoner::RHI::ERHIResult Result = Submissions.back()->ObserveCompletion(TimeoutMicroseconds);
    if (Diagnostics)
    {
        MarkCompletion(*Diagnostics, Result == Stoner::RHI::ERHIResult::Success ? "fallback submission completed" : "fallback submission completion did not finish");
    }
    return Result;
}

void FVulkanQueue::ConfigureCompletionInjection(FVulkanCompletionInjectionConfig InInjection) noexcept
{
    CompletionInjection = InInjection;
}

void FVulkanQueue::Invalidate() noexcept
{
    bValid = false;
    for (const auto& Submission : Submissions)
    {
        if (Submission)
        {
            Submission->Invalidate();
        }
    }
}

} // namespace Stoner::Backend::Vulkan
