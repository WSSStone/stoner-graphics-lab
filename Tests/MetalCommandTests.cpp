#include "MetalCommandTests.h"

#include "Core/SGPlatform.h"
#if SG_PLATFORM_MAC
#include "FMetalCommandBuffer.h"
#include "FMetalDeviceOwnerState.h"
#include "FMetalSubmission.h"
#include "FMetalSynchronization.h"
#endif

#include <iostream>

namespace
{

#if SG_PLATFORM_MAC
using namespace Stoner;
using namespace Stoner::Core;
using namespace Stoner::RHI;
#endif

void Record(FMetalCommandTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

#if SG_PLATFORM_MAC
void TestCommandState(FMetalCommandTestResult& Result)
{
    auto Owner = MakeShared<Backend::Metal::Private::FMetalDeviceOwnerState>(101);
    Backend::Metal::Private::FMetalCommandBuffer Commands(
        Owner, ERHIQueueType::Graphics);
    TArray<Backend::Metal::Private::FMetalCommandRecord> Snapshot;
    const bool StateMachine = Commands.GetState() == ERHICommandBufferState::Idle &&
        Commands.Begin() == ERHIResult::Success &&
        Commands.Begin() == ERHIResult::InvalidState &&
        Commands.RecordBarrier() == ERHIResult::Success &&
        Commands.End() == ERHIResult::Success &&
        Commands.PrepareSubmission(Snapshot) && Snapshot.size() == 1 &&
        Commands.GetState() == ERHICommandBufferState::Submitted &&
        Commands.Reset() == ERHIResult::InvalidState;
    Commands.CompleteSubmission();
    Record(Result,
        StateMachine &&
            Commands.GetState() == ERHICommandBufferState::Resettable &&
            Commands.Reset() == ERHIResult::Success,
        "command recording has deterministic submit and reset states");

    Backend::Metal::Private::FMetalCommandBuffer Transfer(
        Owner, ERHIQueueType::Transfer);
    Record(Result,
        Transfer.Begin() == ERHIResult::Success &&
            Transfer.RecordDispatch(1, 1, 1) == ERHIResult::Unsupported &&
            Transfer.End() == ERHIResult::Success,
        "transfer command buffers reject compute commands");

    Backend::Metal::Private::FMetalCommandBuffer Stale(
        Owner, ERHIQueueType::Graphics);
    Owner->StopAdmission();
    Owner->AdvanceGeneration();
    Record(Result,
        Stale.Begin() == ERHIResult::InvalidState,
        "stale command buffers reject work after owner generation changes");
}

void TestSynchronization(FMetalCommandTestResult& Result)
{
    auto Owner = MakeShared<Backend::Metal::Private::FMetalDeviceOwnerState>(102);
    Backend::Metal::Private::FMetalFence Fence(Owner, nullptr, false);
    const uint64 Epoch = Fence.ReserveSubmissionSignal();
    Fence.CompleteSubmissionSignal(Epoch + 1, true);
    const bool FenceEpoch = Epoch == 1 && !Fence.IsSignaled() &&
        Fence.Reset() == ERHIResult::InvalidState;
    Fence.CompleteSubmissionSignal(Epoch, true);
    Record(Result,
        FenceEpoch && Fence.Wait() == ERHIResult::Success &&
            Fence.GetState() == ERHIFenceState::Waited &&
            Fence.Reset() == ERHIResult::Success &&
            Fence.Wait(100) == ERHIResult::Timeout &&
            Fence.Signal() == ERHIResult::Success,
        "fence epochs reject stale completion and honor timeout/reset");

    Backend::Metal::Private::FMetalSemaphore Semaphore(Owner, nullptr);
    Record(Result,
        Semaphore.Consume() == ERHIResult::NotReady &&
            Semaphore.Signal() == ERHIResult::Success &&
            Semaphore.Signal() == ERHIResult::InvalidState &&
            Semaphore.CanWaitForSubmission(Owner) &&
            Semaphore.GetWaitEpoch() == 1 &&
            Semaphore.Consume() == ERHIResult::Success &&
            Semaphore.Reset() == ERHIResult::Success,
        "semaphore signal consumption is binary over monotonic epochs");
}

void TestCompletedSubmissionReleasesSynchronization(
    FMetalCommandTestResult& Result)
{
    auto Owner = MakeShared<Backend::Metal::Private::FMetalDeviceOwnerState>(103);
    auto Commands = MakeShared<Backend::Metal::Private::FMetalCommandBuffer>(
        Owner, ERHIQueueType::Graphics);
    auto Fence = MakeShared<Backend::Metal::Private::FMetalFence>(
        Owner, nullptr, false);
    TWeakPtr<Backend::Metal::Private::FMetalFence> WeakFence = Fence;
    const uint64 Epoch = Fence->ReserveSubmissionSignal();
    Backend::Metal::Private::FMetalSubmission Submission(
        Owner, Commands, {}, {}, {}, {}, Fence, Epoch);
    Fence.reset();
    Commands.reset();

    Submission.Complete(true);
    Record(Result,
        WeakFence.expired() && Submission.IsComplete() &&
            Submission.Wait() == ERHIResult::Success,
        "completed submissions release retained synchronization ownership");
}
#endif

} // namespace

FMetalCommandTestResult RunMetalCommandTests()
{
    FMetalCommandTestResult Result;
#if SG_PLATFORM_MAC
    TestCommandState(Result);
    TestSynchronization(Result);
    TestCompletedSubmissionReleasesSynchronization(Result);
#else
    Record(Result, true, "Metal command implementation is excluded off macOS");
#endif
    return Result;
}
