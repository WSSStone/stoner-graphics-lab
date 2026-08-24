#include "FProductionSubmissionHarness.h"

#include "RHI/IRHICommandQueue.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIFence.h"

namespace Stoner::Demo
{

RHI::ERHIResult FProductionSubmissionHarness::Initialize(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device)
{
    Release();
    if (!Device) return RHI::ERHIResult::InvalidState;
    auto CreatedQueue = Device->CreateCommandQueue(
        RHI::ERHIQueueType::Graphics);
    auto CreatedFence = Device->CreateFence();
    if (!CreatedQueue.Succeeded() || !CreatedFence.Succeeded())
        return RHI::ERHIResult::Failed;
    Queue = std::move(CreatedQueue.Object);
    Fence = std::move(CreatedFence.Object);
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FProductionSubmissionHarness::SubmitAndWait(
    const Core::TSharedPtr<RHI::IRHICommandBuffer>& Commands,
    Core::uint64 TimeoutMicroseconds)
{
    if (!Queue || !Fence || !Commands)
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

void FProductionSubmissionHarness::Release() noexcept
{
    Fence.reset();
    Queue.reset();
}

} // namespace Stoner::Demo
