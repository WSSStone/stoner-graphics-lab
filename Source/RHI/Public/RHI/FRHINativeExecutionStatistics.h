#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::RHI
{

// Monotonic per-device observations at native operation sites. Mapping upload
// memory is intentionally separate from CPU access to readback memory. Counts
// describe calls, including failed calls, and are not timing/profiling samples.
struct FRHINativeExecutionStatistics
{
    bool bAvailable = false;
    Core::uint64 ImageReadbackCopyCount = 0;
    Core::uint64 ReadbackMapCount = 0;
    Core::uint64 ReadbackWaitCount = 0;
    Core::uint64 FenceWaitCallCount = 0;
    Core::uint64 QueueIdleCallCount = 0;
    Core::uint64 DeviceIdleCallCount = 0;
    Core::uint64 SubmittedRenderCount = 0;
    Core::uint64 SuccessfulRenderCompletionCount = 0;
    Core::uint64 ProvenPresentationReleaseCount = 0;
    // Current retained native submission records; this is a gauge, not a total.
    Core::uint64 RetainedSubmissionOwnerCount = 0;
};

struct FRHINativePresentationStatistics
{
    bool bAvailable = false;
    Core::uint64 ActiveGeneration = 0;
    Core::uint64 RetiringGeneration = 0;
    Core::uint32 ActiveImageCount = 0;
    Core::uint32 RetiringImageCount = 0;
    Core::uint32 PendingAcquireCount = 0;
    Core::uint32 AcquisitionRecordCount = 0;
    // Presentation-related image owners, including unpublished native acquisitions.
    Core::uint32 PresentationOwnerCount = 0;
    Core::uint64 EstimatedColorBytes = 0;
    Core::uint64 PeakEstimatedColorBytes = 0;
    Core::uint32 PreCleanupPresentationOwners = 0;
    Core::uint32 ResidualNativeOwners = 0;
    Core::uint32 AbandonedNativeOwners = 0;
    Core::uint64 TerminalIdleCallCount = 0;
    Core::uint64 TerminalIdleNanoseconds = 0;
    Core::int32 TerminalIdleNativeResult = 0;
    bool bTerminalIdleCompleted = false;
};

} // namespace Stoner::RHI
