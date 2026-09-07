#include "FProductionSubmissionHarness.h"

#include "RHI/IRHICommandBuffer.h"
#include "RHI/IRHICommandQueue.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIFence.h"

#include <algorithm>
#include <new>

namespace Stoner::Demo
{

RHI::ERHIResult FProductionSubmissionHarness::Initialize(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device)
{
    if (!PendingDeferredSubmissions.empty())
        return RHI::ERHIResult::NotReady;
    (void)Release();
    if (!Device) return RHI::ERHIResult::InvalidState;
    auto CreatedQueue = Device->CreateCommandQueue(
        RHI::ERHIQueueType::Graphics);
    auto CreatedFence = Device->CreateFence();
    if (!CreatedQueue.Succeeded() || !CreatedFence.Succeeded())
        return RHI::ERHIResult::Failed;
    Queue = std::move(CreatedQueue.Object);
    Fence = std::move(CreatedFence.Object);
    try
    {
        PendingDeferredSubmissions.reserve(2);
    }
    catch (const std::bad_alloc&)
    {
        Fence.reset();
        Queue.reset();
        return RHI::ERHIResult::Failed;
    }
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FProductionSubmissionHarness::SubmitAndWait(
    const Core::TSharedPtr<RHI::IRHICommandBuffer>& Commands,
    Core::uint64 TimeoutMicroseconds)
{
    if (!Queue || !Fence || !Commands ||
        !PendingDeferredSubmissions.empty())
        return RHI::ERHIResult::InvalidState;
    RHI::ERHIResult Result = Queue->Submit(Commands, {}, {}, Fence);
    if (Result == RHI::ERHIResult::Success)
        Result = Fence->Wait(TimeoutMicroseconds);
    if (Result == RHI::ERHIResult::Success)
        Result = Queue->WaitIdle();
    if (Result == RHI::ERHIResult::Success)
        Result = Fence->Reset();
    return Result;
}

FProductionDeferredSubmissionResult
FProductionSubmissionHarness::SubmitDeferred(
    const Core::TSharedPtr<RHI::IRHICommandBuffer>& Commands,
    const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& WaitSemaphores,
    const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& SignalSemaphores,
    const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence)
{
    FProductionDeferredSubmissionResult Out;
    if (!Queue || !Commands || !CompletionFence)
        return Out;
    if (PendingDeferredSubmissions.size() >= 2)
    {
        Out.Result = RHI::ERHIResult::NotReady;
        return Out;
    }
    if (Commands->GetState() != RHI::ERHICommandBufferState::Completed)
        return Out;
    const auto PendingCommand = std::find_if(
        PendingDeferredSubmissions.begin(), PendingDeferredSubmissions.end(),
        [&Commands](const FPendingDeferredSubmission& Pending)
        {
            return Pending.CommandBuffer == Commands;
        });
    if (PendingCommand != PendingDeferredSubmissions.end())
        return Out;
    if (FindPending(CompletionFence) != nullptr)
        return Out;

    // Reserve and populate the complete owner record before entering the
    // native queue. Once SubmitDeferred can have a native side effect, no
    // container allocation or owner copy remains that could lose the record.
    const Core::usize InitialPendingCount =
        PendingDeferredSubmissions.size();
    try
    {
        PendingDeferredSubmissions.emplace_back();
        FPendingDeferredSubmission& Pending =
            PendingDeferredSubmissions.back();
        Pending.CommandBuffer = Commands;
        Pending.CompletionFence = CompletionFence;
        Pending.WaitSemaphores = WaitSemaphores;
        Pending.SignalSemaphores = SignalSemaphores;
    }
    catch (const std::bad_alloc&)
    {
        if (PendingDeferredSubmissions.size() > InitialPendingCount)
            PendingDeferredSubmissions.pop_back();
        Out.Result = RHI::ERHIResult::Failed;
        return Out;
    }

    FPendingDeferredSubmission& Pending =
        PendingDeferredSubmissions.back();
    RHI::ERHIResult Result = RHI::ERHIResult::Failed;
    try
    {
        Result = Queue->SubmitDeferred(Pending.CommandBuffer,
            Pending.WaitSemaphores, Pending.SignalSemaphores,
            Pending.CompletionFence);
    }
    catch (...)
    {
        Result = RHI::ERHIResult::Failed;
    }
    const bool bAccepted = Result == RHI::ERHIResult::Success ||
        Commands->GetState() == RHI::ERHICommandBufferState::Submitted;
    if (!bAccepted)
    {
        PendingDeferredSubmissions.pop_back();
        Out.Result = Result;
        return Out;
    }
    Pending.bSubmissionAccepted = true;
    Pending.FirstFailure = Result == RHI::ERHIResult::Success
        ? RHI::ERHIResult::Success : Result;
    Out.Result = Result;
    Out.bSubmissionAccepted = true;
    return Out;
}

FProductionDeferredSubmissionResult FProductionSubmissionHarness::PollDeferred(
    const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence) noexcept
{
    FProductionDeferredSubmissionResult Out;
    FPendingDeferredSubmission* Pending = FindPending(CompletionFence);
    if (!Pending)
        return Out;
    Out.bSubmissionAccepted = Pending->bSubmissionAccepted;
    if (Pending->bCompletionObserved)
    {
        Out.Result = Pending->FirstFailure;
        Out.bCompletionObserved = true;
        return Out;
    }

    RHI::ERHIResult Result = RHI::ERHIResult::Failed;
    try
    {
        // Wait(0) is the RHI's bounded, nonblocking fence status query.
        Result = CompletionFence->Wait(0);
    }
    catch (...)
    {
        Result = RHI::ERHIResult::Failed;
    }
    if (Result == RHI::ERHIResult::Success)
    {
        Pending->bCompletionObserved = true;
        Out.bCompletionObserved = true;
        Out.Result = Pending->FirstFailure;
        return Out;
    }
    if (Result != RHI::ERHIResult::NotReady &&
        Pending->FirstFailure == RHI::ERHIResult::Success)
        Pending->FirstFailure = Result;
    Out.Result = Pending->FirstFailure == RHI::ERHIResult::Success
        ? Result : Pending->FirstFailure;
    return Out;
}

FProductionDeferredSubmissionResult
FProductionSubmissionHarness::RetireDeferred(
    const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence) noexcept
{
    FProductionDeferredSubmissionResult Out;
    const auto It = std::find_if(PendingDeferredSubmissions.begin(),
        PendingDeferredSubmissions.end(),
        [&CompletionFence](const FPendingDeferredSubmission& Pending)
        {
            return Pending.CompletionFence == CompletionFence;
        });
    if (It == PendingDeferredSubmissions.end())
        return Out;
    Out.bSubmissionAccepted = It->bSubmissionAccepted;
    if (!It->bCompletionObserved)
    {
        Out.Result = It->FirstFailure == RHI::ERHIResult::Success
            ? RHI::ERHIResult::NotReady : It->FirstFailure;
        return Out;
    }
    RHI::ERHIResult Reset = RHI::ERHIResult::Failed;
    try
    {
        // A fence is reset only after an observed completion, never merely
        // because polling returned a failure or NotReady.
        Reset = CompletionFence->Reset();
    }
    catch (...)
    {
        Reset = RHI::ERHIResult::Failed;
    }
    Out.bCompletionObserved = true;
    if (Reset == RHI::ERHIResult::NotReady)
    {
        // Reset is a separate retryable lifecycle operation. Completion is
        // already proven and must remain visible to the caller.
        Out.Result = It->FirstFailure == RHI::ERHIResult::Success
            ? RHI::ERHIResult::NotReady : It->FirstFailure;
        return Out;
    }
    if (Reset != RHI::ERHIResult::Success)
    {
        if (It->FirstFailure == RHI::ERHIResult::Success)
            It->FirstFailure = Reset;
        Out.Result = It->FirstFailure;
        return Out;
    }
    Out.Result = It->FirstFailure;
    Out.bRetired = true;
    PendingDeferredSubmissions.erase(It);
    return Out;
}

FProductionSubmissionHarness::FPendingDeferredSubmission*
FProductionSubmissionHarness::FindPending(
    const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence) noexcept
{
    if (!CompletionFence) return nullptr;
    const auto It = std::find_if(PendingDeferredSubmissions.begin(),
        PendingDeferredSubmissions.end(),
        [&CompletionFence](FPendingDeferredSubmission& Pending)
        {
            return Pending.CompletionFence == CompletionFence;
        });
    return It == PendingDeferredSubmissions.end() ? nullptr : &*It;
}

RHI::ERHIResult FProductionSubmissionHarness::Release() noexcept
{
    if (!PendingDeferredSubmissions.empty())
        return RHI::ERHIResult::NotReady;
    Fence.reset();
    Queue.reset();
    PendingDeferredSubmissions.clear();
    return RHI::ERHIResult::Success;
}

} // namespace Stoner::Demo
