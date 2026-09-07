#include "VulkanRHI/FVulkanQueue.h"

#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanNativeContext.h"
#include "VulkanRHI/FVulkanSemaphore.h"

#include <algorithm>
#include <new>
#include <stdexcept>

namespace Stoner::Backend::Vulkan
{

FVulkanQueue::FVulkanQueue(
    Stoner::RHI::ERHIQueueType InQueueType,
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner,
    FVulkanDiagnostics* InDiagnostics,
    FVulkanCompletionInjectionConfig InInjection,
    Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext) noexcept
    : QueueType(InQueueType)
    , Owner(std::move(InOwner))
    , Diagnostics(InDiagnostics)
    , CompletionInjection(InInjection)
    , NativeContext(std::move(InNativeContext))
{
    if (NativeContext)
    {
        NativeContext->AdoptDeviceOwnerState(Owner);
    }
}

Stoner::RHI::ERHIQueueType FVulkanQueue::GetQueueType() const noexcept
{
    return QueueType;
}

Stoner::Core::uint32 FVulkanQueue::GetSubmittedCommandBufferCount() const noexcept
{
    return SubmittedCommandBufferCount;
}

Stoner::Core::uint64 FVulkanQueue::GetLastDeferredSubmissionId() const noexcept
{
    return LastDeferredSubmissionId;
}

bool FVulkanQueue::IsValid() const noexcept
{
    return bValid && Owner && Owner->bActive;
}

Stoner::RHI::ERHIResult FVulkanQueue::Submit(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& CommandBuffer,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& WaitSemaphores,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& SignalSemaphores,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>& Fence)
{
    if (!IsValid())
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

    auto VulkanCommandBuffer = std::dynamic_pointer_cast<FVulkanCommandBuffer>(CommandBuffer);
    if (!VulkanCommandBuffer || !VulkanCommandBuffer->BelongsTo(Owner))
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    for (std::size_t Index = 0; Index < WaitSemaphores.size(); ++Index)
    {
        const auto VulkanSemaphore =
            std::dynamic_pointer_cast<FVulkanSemaphore>(WaitSemaphores[Index]);
        if (!VulkanSemaphore || !VulkanSemaphore->BelongsTo(Owner))
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        // Lab-bound acquire semaphores carry native presentation ownership and
        // must only enter the deferred overload that forwards their native
        // wait to vkQueueSubmit. The legacy Submit path has no native wait
        // parameters and would otherwise consume the logical semaphore only.
        if (VulkanSemaphore->GetLabBindingToken() != 0)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        if (!VulkanSemaphore->CanConsumeForSubmission())
        {
            return Stoner::RHI::ERHIResult::NotReady;
        }
        for (std::size_t Previous = 0; Previous < Index; ++Previous)
        {
            if (WaitSemaphores[Previous].get() == WaitSemaphores[Index].get())
            {
                return Stoner::RHI::ERHIResult::InvalidState;
            }
        }
    }

    for (std::size_t Index = 0; Index < SignalSemaphores.size(); ++Index)
    {
        const auto VulkanSemaphore =
            std::dynamic_pointer_cast<FVulkanSemaphore>(SignalSemaphores[Index]);
        if (!VulkanSemaphore || !VulkanSemaphore->BelongsTo(Owner) ||
            !VulkanSemaphore->CanSignalForSubmission())
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        if (VulkanSemaphore->GetLabBindingToken() != 0)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        for (const auto& WaitSemaphore : WaitSemaphores)
        {
            if (WaitSemaphore.get() == SignalSemaphores[Index].get())
            {
                return Stoner::RHI::ERHIResult::InvalidState;
            }
        }
        for (std::size_t Previous = 0; Previous < Index; ++Previous)
        {
            if (SignalSemaphores[Previous].get() ==
                SignalSemaphores[Index].get())
            {
                return Stoner::RHI::ERHIResult::InvalidState;
            }
        }
    }

    Stoner::Core::TSharedPtr<FVulkanFence> VulkanFence;
    if (Fence)
    {
        VulkanFence = std::dynamic_pointer_cast<FVulkanFence>(Fence);
        if (!VulkanFence || !VulkanFence->BelongsTo(Owner) ||
            !VulkanFence->CanSignalForSubmission())
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
    }

    if (NativeContext && NativeContext->IsAvailable())
    {
        const Stoner::RHI::ERHIResult NativeResult =
            NativeContext->ExecuteRecordedCommands(VulkanCommandBuffer);
        if (NativeResult != Stoner::RHI::ERHIResult::Success)
        {
            if (Diagnostics)
            {
                MarkSubmission(*Diagnostics, "native Vulkan command execution failed");
            }
            return NativeResult;
        }
    }

    Stoner::Core::TSharedPtr<FVulkanCommandSubmission> Submission;
    try
    {
        Submission.reset(new FVulkanCommandSubmission(
            VulkanCommandBuffer,
            EVulkanSubmissionMode::DeterministicFallback,
            CompletionInjection));
        Submissions.push_back(Submission);
    }
    catch (const std::bad_alloc&)
    {
        if (Diagnostics)
        {
            MarkSubmission(*Diagnostics, "submission tracking allocation failed");
        }
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        if (Diagnostics)
        {
            MarkSubmission(*Diagnostics, "submission tracking capacity exceeded");
        }
        return Stoner::RHI::ERHIResult::Unavailable;
    }

    if (VulkanCommandBuffer->MarkSubmitted() != Stoner::RHI::ERHIResult::Success)
    {
        Submissions.pop_back();
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    for (const auto& Semaphore : WaitSemaphores)
    {
        std::dynamic_pointer_cast<FVulkanSemaphore>(Semaphore)
            ->CommitConsumeForSubmission();
    }
    for (const auto& Semaphore : SignalSemaphores)
    {
        std::dynamic_pointer_cast<FVulkanSemaphore>(Semaphore)
            ->CommitSignalForSubmission();
    }
    if (VulkanFence)
    {
        VulkanFence->CommitSignalForSubmission();
    }
    ++SubmittedCommandBufferCount;

    if (Diagnostics)
    {
        MarkSubmission(*Diagnostics, NativeContext && NativeContext->IsAvailable()
            ? "native Vulkan submission completed"
            : "deterministic fallback submission recorded; no real GPU execution occurred");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanQueue::SubmitDeferred(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& CommandBuffer,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& WaitSemaphores,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& SignalSemaphores,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>& CompletionFence)
{
    if (!IsValid())
        return Stoner::RHI::ERHIResult::InvalidState;
    if (WaitSemaphores.size() > 1 || SignalSemaphores.size() > 1 ||
        (WaitSemaphores.empty() && !SignalSemaphores.empty()))
        return Stoner::RHI::ERHIResult::Unsupported;
    if (!CompletionFence || !CommandBuffer ||
        CommandBuffer->GetState() != Stoner::RHI::ERHICommandBufferState::Completed ||
        CommandBuffer->GetRecordedCommandCount() == 0)
        return Stoner::RHI::ERHIResult::InvalidState;
    if (CommandBuffer->GetCompatibleQueueType() != QueueType)
        return Stoner::RHI::ERHIResult::Unsupported;

    const auto VulkanCommandBuffer =
        std::dynamic_pointer_cast<FVulkanCommandBuffer>(CommandBuffer);
    const auto VulkanFence =
        std::dynamic_pointer_cast<FVulkanFence>(CompletionFence);
    if (!VulkanCommandBuffer || !VulkanCommandBuffer->BelongsTo(Owner) ||
        !VulkanFence || !VulkanFence->BelongsTo(Owner) ||
        !VulkanFence->CanSignalForSubmission())
        return Stoner::RHI::ERHIResult::InvalidState;
    if (!NativeContext || !NativeContext->IsAvailable())
        return Stoner::RHI::ERHIResult::Unsupported;
    if (NativeContext->GetPendingDeferredSubmissionCount() >= 2)
        return Stoner::RHI::ERHIResult::NotReady;
    if (VulkanCommandBuffer->MarkSubmitted() !=
        Stoner::RHI::ERHIResult::Success)
        return Stoner::RHI::ERHIResult::InvalidState;

    Stoner::Core::uint64 SubmissionId = 0;
    const auto Result = (WaitSemaphores.empty()
        ? NativeContext->SubmitDeferredCommands(
            VulkanCommandBuffer, VulkanFence, SubmissionId)
        : NativeContext->SubmitDeferredCommands(
            VulkanCommandBuffer, VulkanFence,
            WaitSemaphores.front(),
            SignalSemaphores.empty() ? nullptr : SignalSemaphores.front(),
            SubmissionId));
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        if (SubmissionId == 0)
        {
            VulkanCommandBuffer->RollbackSubmitted();
        }
        else
        {
            for (const auto& Semaphore : WaitSemaphores)
            {
                std::dynamic_pointer_cast<FVulkanSemaphore>(Semaphore)
                    ->CommitConsumeForSubmission();
            }
            for (const auto& Semaphore : SignalSemaphores)
            {
                std::dynamic_pointer_cast<FVulkanSemaphore>(Semaphore)
                    ->CommitSignalForSubmission();
            }
        }
        return Result;
    }
    for (const auto& Semaphore : WaitSemaphores)
    {
        std::dynamic_pointer_cast<FVulkanSemaphore>(Semaphore)
            ->CommitConsumeForSubmission();
    }
    for (const auto& Semaphore : SignalSemaphores)
    {
        std::dynamic_pointer_cast<FVulkanSemaphore>(Semaphore)
            ->CommitSignalForSubmission();
    }
    LastDeferredSubmissionId = SubmissionId;
    ++SubmittedCommandBufferCount;
    if (Diagnostics)
        MarkSubmission(*Diagnostics,
            "native Vulkan deferred submission accepted; completion remains fence-owned");
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanQueue::WaitIdle()
{
    if (!IsValid())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (NativeContext && NativeContext->IsAvailable())
    {
        const auto Result = NativeContext->WaitAllDeferredSubmissions();
        if (Result != Stoner::RHI::ERHIResult::Success)
            return Result;
        LastDeferredSubmissionId = 0;
    }
    for (const auto& Submission : Submissions)
    {
        if (Submission &&
            Submission->GetCompletionState() != EVulkanCompletionState::Completed &&
            Submission->GetCompletionState() != EVulkanCompletionState::Invalidated)
        {
            const Stoner::RHI::ERHIResult Result = Submission->CompleteForWaitIdle();
            if (Result != Stoner::RHI::ERHIResult::Success)
            {
                return Result;
            }
        }
    }
    if (!Submissions.empty()) bHasCompletedSubmission = true;
    Submissions.erase(
        std::remove_if(Submissions.begin(), Submissions.end(),
            [](const auto& Submission)
            {
                return !Submission ||
                    Submission->GetCompletionState() ==
                        EVulkanCompletionState::Completed ||
                    Submission->GetCompletionState() ==
                        EVulkanCompletionState::Invalidated;
            }),
        Submissions.end());
    if (Diagnostics)
    {
        MarkCompletion(*Diagnostics, "queue wait idle completed fallback submissions");
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanQueue::ObserveLastSubmissionCompletion(Stoner::Core::uint64 TimeoutMicroseconds) noexcept
{
    if (!IsValid())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (LastDeferredSubmissionId != 0 && NativeContext &&
        NativeContext->IsAvailable())
    {
        const auto Result = NativeContext->WaitDeferredSubmission(
            LastDeferredSubmissionId, TimeoutMicroseconds);
        if (Result == Stoner::RHI::ERHIResult::Success)
        {
            LastDeferredSubmissionId = 0;
            bHasCompletedSubmission = true;
        }
        return Result;
    }
    if (Submissions.empty())
        return bHasCompletedSubmission
            ? Stoner::RHI::ERHIResult::Success
            : Stoner::RHI::ERHIResult::InvalidState;
    if (!Submissions.back()) return Stoner::RHI::ERHIResult::InvalidState;
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
    Owner.reset();
    NativeContext.reset();
    Diagnostics = nullptr;
    bHasCompletedSubmission = false;
    LastDeferredSubmissionId = 0;
}

} // namespace Stoner::Backend::Vulkan
