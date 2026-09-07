#include "FVulkanLabPresentationPolicy.h"

#include "RHI/FRHIFormatInfo.h"

#include <limits>
#include <new>

namespace Stoner::Backend::Vulkan::Private
{

namespace
{

using Stoner::Core::uint32;
using Stoner::Core::uint64;
using Stoner::RHI::ERHIPresentationRetirementMode;
using Stoner::RHI::ERHIPresentationRetirementReason;

constexpr uint32 MaxDrawableAxis = 4096;
constexpr uint64 MaxDrawablePixels = 7'864'320;

[[nodiscard]] FVulkanLabPresentationPolicyCandidate MakeCandidate(
    EVulkanLabPresentationCandidate Candidate,
    ERHIPresentationRetirementReason Reason,
    const FVulkanLabPresentationCapabilityObservation& Observation) noexcept
{
    FVulkanLabPresentationPolicyCandidate Result;
    Result.Candidate = Candidate;
    Result.Reason = Reason;
    Result.bRequiresOptionalEnablement =
        Candidate == EVulkanLabPresentationCandidate::PresentationFence;
    Result.bExtensionAdvertised = Observation.bExtensionAdvertised;
    Result.bFeatureAdvertised = Observation.bFeatureAdvertised;
    Result.bInstanceDependenciesAvailable =
        Observation.bInstanceDependenciesAvailable;
    Result.bEntryPointsResolved = Observation.bEntryPointsResolved;
    Result.NativeQueryResult = Observation.NativeQueryResult;
    try
    {
        Result.QueryFailure = Observation.QueryFailure;
    }
    catch (const std::bad_alloc&)
    {
        // Keep the original native code and typed reason even if copying
        // optional diagnostic text cannot allocate during failure handling.
    }
    return Result;
}

[[nodiscard]] FVulkanLabPresentationPolicySelection MakeSelection(
    ERHIPresentationRetirementMode Mode,
    ERHIPresentationRetirementReason Reason,
    const FVulkanLabPresentationPolicyCandidate& Candidate) noexcept
{
    FVulkanLabPresentationPolicySelection Result;
    Result.Mode = Mode;
    Result.Reason = Reason;
    Result.bExtensionAdvertised = Candidate.bExtensionAdvertised;
    Result.bFeatureAdvertised = Candidate.bFeatureAdvertised;
    Result.bInstanceDependenciesAvailable =
        Candidate.bInstanceDependenciesAvailable;
    Result.bEntryPointsResolved = Candidate.bEntryPointsResolved;
    Result.NativeQueryResult = Candidate.NativeQueryResult;
    try
    {
        Result.QueryFailure = Candidate.QueryFailure;
    }
    catch (const std::bad_alloc&)
    {
        // The typed reason and native code remain available without text.
    }
    return Result;
}

} // namespace

FVulkanLabPresentationPolicyCandidate
FVulkanLabPresentationPolicy::SelectCandidate(
    const FVulkanLabPresentationPolicyRequest& Request,
    const FVulkanLabPresentationCapabilityObservation& Observation) noexcept
{
    if (Request.bForceAcquireHistory ||
        !Request.bAllowOptionalPresentationFence)
    {
        return MakeCandidate(
            EVulkanLabPresentationCandidate::AcquireHistory,
            ERHIPresentationRetirementReason::ForcedOff,
            Observation);
    }

    switch (Observation.QueryState)
    {
    case EVulkanLabCapabilityQueryState::NotAttempted:
    case EVulkanLabCapabilityQueryState::Unavailable:
        return MakeCandidate(
            EVulkanLabPresentationCandidate::AcquireHistory,
            ERHIPresentationRetirementReason::QueryUnavailable,
            Observation);
    case EVulkanLabCapabilityQueryState::Failed:
        return MakeCandidate(
            EVulkanLabPresentationCandidate::None,
            ERHIPresentationRetirementReason::QueryFailed,
            Observation);
    case EVulkanLabCapabilityQueryState::Succeeded:
        break;
    }

    if (!Observation.bExtensionAdvertised)
    {
        return MakeCandidate(
            EVulkanLabPresentationCandidate::AcquireHistory,
            ERHIPresentationRetirementReason::ExtensionAbsent,
            Observation);
    }
    if (!Observation.bFeatureAdvertised)
    {
        return MakeCandidate(
            EVulkanLabPresentationCandidate::AcquireHistory,
            ERHIPresentationRetirementReason::FeatureAbsent,
            Observation);
    }
    if (!Observation.bInstanceDependenciesAvailable)
    {
        return MakeCandidate(
            EVulkanLabPresentationCandidate::AcquireHistory,
            ERHIPresentationRetirementReason::InstanceDependencyAbsent,
            Observation);
    }
    // This is only a candidate.  The native device must be created with the
    // optional feature.  Device entry points are resolved after creation;
    // CompleteEnablement receives that actual result before publishing
    // PresentationFence.
    return MakeCandidate(
        EVulkanLabPresentationCandidate::PresentationFence,
        ERHIPresentationRetirementReason::Preferred,
        Observation);
}

FVulkanLabPresentationPolicySelection
FVulkanLabPresentationPolicy::CompleteEnablement(
    const FVulkanLabPresentationPolicyCandidate& Candidate,
    EVulkanLabOptionalEnablementState Enablement,
    bool bUsedEntryPointsResolved,
    bool bFallbackRetryAlreadyConsumed) noexcept
{
    if (!Candidate.IsValid())
    {
        return MakeSelection(
            ERHIPresentationRetirementMode::Unknown, Candidate.Reason, Candidate);
    }

    if (Candidate.Candidate == EVulkanLabPresentationCandidate::AcquireHistory)
    {
        return MakeSelection(
            ERHIPresentationRetirementMode::AcquireHistory,
            Candidate.Reason,
            Candidate);
    }

    if (Candidate.Candidate != EVulkanLabPresentationCandidate::PresentationFence)
    {
        return {};
    }

    if (Enablement == EVulkanLabOptionalEnablementState::Succeeded &&
        bUsedEntryPointsResolved)
    {
        FVulkanLabPresentationPolicySelection Result = MakeSelection(
            ERHIPresentationRetirementMode::PresentationFence,
            ERHIPresentationRetirementReason::Preferred,
            Candidate);
        Result.bOptionalEnabled = true;
        Result.bEntryPointsResolved = bUsedEntryPointsResolved;
        return Result;
    }

    if ((Enablement == EVulkanLabOptionalEnablementState::Failed ||
         (Enablement == EVulkanLabOptionalEnablementState::Succeeded &&
          !bUsedEntryPointsResolved)) &&
        !bFallbackRetryAlreadyConsumed)
    {
        FVulkanLabPresentationPolicySelection Result = MakeSelection(
            ERHIPresentationRetirementMode::AcquireHistory,
            Enablement == EVulkanLabOptionalEnablementState::Failed
                ? ERHIPresentationRetirementReason::OptionalEnablementFailed
                : ERHIPresentationRetirementReason::EntryPointsUnavailable,
            Candidate);
        Result.bRetryWithoutOptional = true;
        Result.bRetryPending = true;
        Result.bEntryPointsResolved = bUsedEntryPointsResolved;
        return Result;
    }

    return MakeSelection(
        ERHIPresentationRetirementMode::Unknown,
        bFallbackRetryAlreadyConsumed
            ? ERHIPresentationRetirementReason::OptionalEnablementRetryExhausted
            : (bUsedEntryPointsResolved
                ? ERHIPresentationRetirementReason::Unknown
                : ERHIPresentationRetirementReason::EntryPointsUnavailable),
        Candidate);
}

FVulkanLabPresentationPolicySelection
FVulkanLabPresentationPolicy::CompleteFallbackRetry(
    const FVulkanLabPresentationPolicySelection& Retry,
    EVulkanLabFallbackDeviceCreationState DeviceCreation) noexcept
{
    if (!Retry.bRetryPending || !Retry.bRetryWithoutOptional)
    {
        return {};
    }

    FVulkanLabPresentationPolicySelection Result;
    try
    {
        Result = Retry;
    }
    catch (const std::bad_alloc&)
    {
        // Scalar evidence preceding QueryFailure was copied; finish the
        // trailing flags so allocation failure cannot publish a pending retry.
        Result.bOptionalEnabled = Retry.bOptionalEnabled;
        Result.bRetryPending = Retry.bRetryPending;
    }
    switch (DeviceCreation)
    {
    case EVulkanLabFallbackDeviceCreationState::NotAttempted:
        // Keep the request explicitly pending.  Its AcquireHistory mode is a
        // requested fallback, not a finalized mode until ordinary device
        // creation succeeds.
        return Result;
    case EVulkanLabFallbackDeviceCreationState::Succeeded:
        Result.Mode = ERHIPresentationRetirementMode::AcquireHistory;
        Result.bRetryWithoutOptional = false;
        Result.bRetryPending = false;
        Result.bOptionalEnabled = false;
        return Result;
    case EVulkanLabFallbackDeviceCreationState::Failed:
        Result.Mode = ERHIPresentationRetirementMode::Unknown;
        Result.Reason =
            ERHIPresentationRetirementReason::FallbackCreationFailed;
        Result.bRetryWithoutOptional = false;
        Result.bRetryPending = false;
        Result.bOptionalEnabled = false;
        return Result;
    }

    return {};
}

FVulkanLabPresentationPolicy::FVulkanLabPresentationPolicy(
    ERHIPresentationRetirementMode Mode) noexcept
    : Mode_(Mode)
{
}

bool FVulkanLabPresentationPolicy::TryEstimateColorBytes(
    const FVulkanLabPresentationGenerationDesc& Desc,
    uint64& OutBytes) noexcept
{
    OutBytes = 0;
    if (!IsValidGenerationDesc(Desc))
    {
        return false;
    }

    const uint64 BytesPerPixel =
        Stoner::RHI::GetRHIFormatInfo(Desc.ColorFormat).BytesPerBlock;
    const uint64 MaxValue = std::numeric_limits<uint64>::max();
    if (static_cast<uint64>(Desc.Width) > MaxValue / Desc.Height)
    {
        return false;
    }
    const uint64 Pixels = static_cast<uint64>(Desc.Width) * Desc.Height;
    if (Pixels > MaxValue / BytesPerPixel)
    {
        return false;
    }
    const uint64 PerImage = Pixels * BytesPerPixel;
    if (static_cast<uint64>(Desc.ImageCount) > MaxValue / PerImage)
    {
        return false;
    }
    OutBytes = PerImage * Desc.ImageCount;
    return OutBytes <= MaxEstimatedColorBytes;
}

bool FVulkanLabPresentationPolicy::IsValidGenerationDesc(
    const FVulkanLabPresentationGenerationDesc& Desc) noexcept
{
    if (Desc.Generation == 0 || Desc.ImageCount == 0 ||
        Desc.ImageCount > MaxImagesPerGeneration || Desc.Width == 0 ||
        Desc.Height == 0 || Desc.Width > MaxDrawableAxis ||
        Desc.Height > MaxDrawableAxis || Desc.ColorFormat ==
            Stoner::RHI::ERHIFormat::Unknown)
    {
        return false;
    }

    if (static_cast<uint64>(Desc.Width) * Desc.Height > MaxDrawablePixels)
    {
        return false;
    }

    const Stoner::RHI::FRHIFormatInfo Info =
        Stoner::RHI::GetRHIFormatInfo(Desc.ColorFormat);
    return Info.IsValid() && !Info.bCompressed && !Info.bDepthStencil &&
        Info.BytesPerBlock != 0;
}

FVulkanLabPresentationPolicy::FGenerationRecord*
FVulkanLabPresentationPolicy::FindGeneration(uint64 Generation) noexcept
{
    for (FGenerationRecord& Record : Generations_)
    {
        if (Record.bOccupied && Record.Desc.Generation == Generation)
        {
            return &Record;
        }
    }
    return nullptr;
}

const FVulkanLabPresentationPolicy::FGenerationRecord*
FVulkanLabPresentationPolicy::FindGeneration(uint64 Generation) const noexcept
{
    for (const FGenerationRecord& Record : Generations_)
    {
        if (Record.bOccupied && Record.Desc.Generation == Generation)
        {
            return &Record;
        }
    }
    return nullptr;
}

FVulkanLabPresentationPolicy::FGenerationRecord*
FVulkanLabPresentationPolicy::FindActiveGeneration() noexcept
{
    for (FGenerationRecord& Record : Generations_)
    {
        if (Record.bOccupied && Record.bActive)
        {
            return &Record;
        }
    }
    return nullptr;
}

const FVulkanLabPresentationPolicy::FGenerationRecord*
FVulkanLabPresentationPolicy::FindActiveGeneration() const noexcept
{
    for (const FGenerationRecord& Record : Generations_)
    {
        if (Record.bOccupied && Record.bActive)
        {
            return &Record;
        }
    }
    return nullptr;
}

FVulkanLabPresentationPolicy::FGenerationRecord*
FVulkanLabPresentationPolicy::FindRetiringGeneration() noexcept
{
    for (FGenerationRecord& Record : Generations_)
    {
        if (Record.bOccupied && Record.bRetiring)
        {
            return &Record;
        }
    }
    return nullptr;
}

const FVulkanLabPresentationPolicy::FGenerationRecord*
FVulkanLabPresentationPolicy::FindRetiringGeneration() const noexcept
{
    for (const FGenerationRecord& Record : Generations_)
    {
        if (Record.bOccupied && Record.bRetiring)
        {
            return &Record;
        }
    }
    return nullptr;
}

FVulkanLabPresentationPolicy::FPresentationRecord*
FVulkanLabPresentationPolicy::FindPresentationRecord(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) noexcept
{
    for (FPresentationRecord& Record : Records_)
    {
        if (Record.bOccupied && Record.Generation == Generation &&
            Record.ImageIndex == ImageIndex &&
            (Record.AcquisitionToken == AcquisitionToken ||
             Record.ReacquisitionToken == AcquisitionToken))
        {
            return &Record;
        }
    }
    return nullptr;
}

const FVulkanLabPresentationPolicy::FPresentationRecord*
FVulkanLabPresentationPolicy::FindPresentationRecord(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) const noexcept
{
    for (const FPresentationRecord& Record : Records_)
    {
        if (Record.bOccupied && Record.Generation == Generation &&
            Record.ImageIndex == ImageIndex &&
            (Record.AcquisitionToken == AcquisitionToken ||
             Record.ReacquisitionToken == AcquisitionToken))
        {
            return &Record;
        }
    }
    return nullptr;
}

bool FVulkanLabPresentationPolicy::IsReacquisitionRecord(
    const FPresentationRecord& Record,
    uint64 AcquisitionToken) const noexcept
{
    return Record.bOccupied && Record.ReacquisitionToken != 0 &&
        Record.ReacquisitionToken == AcquisitionToken;
}

void FVulkanLabPresentationPolicy::PromoteReacquisitionIfReady(
    FPresentationRecord& Record) noexcept
{
    if (!Record.bOccupied || Record.ReacquisitionToken == 0 ||
        !Record.bReacquisitionSynchronized || !Record.bRenderComplete)
    {
        return;
    }

    // The predecessor render domain is complete, so this one fixed-capacity
    // record can now represent the reacquired image without allocating a
    // seventeenth record while an old generation is still retiring.
    const uint64 Token = Record.ReacquisitionToken;
    const bool bCanceled = Record.bReacquisitionCanceled;
    const bool bPresented = Record.bReacquisitionPresented;
    const bool bRenderSubmitted = Record.bReacquisitionRenderSubmitted;
    const bool bRenderComplete = Record.bReacquisitionRenderComplete;
    Record.AcquisitionToken = Token;
    Record.bPresented = bPresented;
    Record.bCanceled = bCanceled;
    Record.bRenderSubmitted = bRenderSubmitted;
    Record.bRenderComplete = bRenderComplete;
    Record.bPresentationRetired = false;
    Record.bReacquisitionObserved = false;
    Record.bReacquisitionSynchronized = false;
    Record.bReacquisitionPresented = false;
    Record.bReacquisitionCanceled = false;
    Record.bReacquisitionRenderSubmitted = false;
    Record.bReacquisitionRenderComplete = false;
    Record.ReacquisitionToken = 0;
}

bool FVulkanLabPresentationPolicy::HasOutstandingImageRecord(
    uint64 Generation,
    uint32 ImageIndex) const noexcept
{
    for (const FPresentationRecord& Record : Records_)
    {
        if (Record.bOccupied && Record.Generation == Generation &&
            Record.ImageIndex == ImageIndex)
        {
            return true;
        }
    }
    return false;
}

bool FVulkanLabPresentationPolicy::CanReleaseRecord(
    const FPresentationRecord& Record) const noexcept
{
    return Record.bOccupied && !Record.bCanceled &&
        Record.ReacquisitionToken == 0 &&
        Record.bRenderComplete && Record.bPresentationRetired;
}

void FVulkanLabPresentationPolicy::ReleaseRecordIfComplete(
    FPresentationRecord& Record) noexcept
{
    if (CanReleaseRecord(Record))
    {
        Record = {};
    }
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::AdmitInitialGeneration(
    const FVulkanLabPresentationGenerationDesc& Desc) noexcept
{
    uint64 EstimatedBytes = 0;
    if (Mode_ == ERHIPresentationRetirementMode::Unknown ||
        bTerminalCleanupStarted_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    if (!IsValidGenerationDesc(Desc))
    {
        return EVulkanLabPresentationPolicyResult::Invalid;
    }
    if (!TryEstimateColorBytes(Desc, EstimatedBytes))
    {
        return EVulkanLabPresentationPolicyResult::BudgetExceeded;
    }
    if (FindGeneration(Desc.Generation) != nullptr ||
        FindActiveGeneration() != nullptr || FindRetiringGeneration() != nullptr ||
        bReplacementInFlight_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }

    for (FGenerationRecord& Record : Generations_)
    {
        if (!Record.bOccupied)
        {
            Record = {};
            Record.bOccupied = true;
            Record.bActive = true;
            Record.Desc = Desc;
            Record.EstimatedColorBytes = EstimatedBytes;
            return EVulkanLabPresentationPolicyResult::Accepted;
        }
    }
    return EVulkanLabPresentationPolicyResult::GenerationLimit;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::QueueReplacement(
    const FVulkanLabPresentationGenerationDesc& Desc) noexcept
{
    if (bTerminalCleanupStarted_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    if (!IsValidGenerationDesc(Desc))
    {
        return EVulkanLabPresentationPolicyResult::Invalid;
    }
    uint64 EstimatedBytes = 0;
    if (!TryEstimateColorBytes(Desc, EstimatedBytes))
    {
        return EVulkanLabPresentationPolicyResult::BudgetExceeded;
    }
    if (FindActiveGeneration() == nullptr)
    {
        return EVulkanLabPresentationPolicyResult::NoActiveGeneration;
    }
    if (FindGeneration(Desc.Generation) != nullptr ||
        (bHasPendingReplacement_ && PendingReplacement_.Generation ==
            Desc.Generation) ||
        (bReplacementInFlight_ && ReplacementInFlight_.Generation ==
            Desc.Generation))
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }

    const FGenerationRecord* Active = FindActiveGeneration();
    if (FindRetiringGeneration() == nullptr && !bReplacementInFlight_ &&
        Active->EstimatedColorBytes >
            MaxEstimatedColorBytes - EstimatedBytes)
    {
        return EVulkanLabPresentationPolicyResult::BudgetExceeded;
    }

    const bool bCoalesced = bHasPendingReplacement_;
    PendingReplacement_ = Desc;
    bHasPendingReplacement_ = true;
    return bCoalesced
        ? EVulkanLabPresentationPolicyResult::Coalesced
        : EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::BeginPendingReplacement() noexcept
{
    if (bTerminalCleanupStarted_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    FGenerationRecord* Active = FindActiveGeneration();
    if (Active == nullptr)
    {
        return EVulkanLabPresentationPolicyResult::NoActiveGeneration;
    }
    if (!bHasPendingReplacement_)
    {
        return EVulkanLabPresentationPolicyResult::NoPendingReplacement;
    }
    if (FindRetiringGeneration() != nullptr || bReplacementInFlight_)
    {
        return EVulkanLabPresentationPolicyResult::GenerationLimit;
    }

    uint64 ReplacementBytes = 0;
    if (!TryEstimateColorBytes(PendingReplacement_, ReplacementBytes) ||
        Active->EstimatedColorBytes >
            MaxEstimatedColorBytes - ReplacementBytes)
    {
        return EVulkanLabPresentationPolicyResult::BudgetExceeded;
    }

    Active->bActive = false;
    Active->bRetiring = true;
    ReplacementInFlight_ = PendingReplacement_;
    bHasPendingReplacement_ = false;
    bReplacementInFlight_ = true;
    bReplacementCreationFailed_ = false;
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::FailPendingReplacementCreation(
    EVulkanLabPresentationPolicyResult Failure) noexcept
{
    if (!bReplacementInFlight_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }

    bReplacementInFlight_ = false;
    if (FGenerationRecord* Retiring = FindRetiringGeneration())
    {
        // vkCreateSwapchainKHR retires oldSwapchain even when creation fails.
        // Keep that predecessor retired and failed so callers cannot resume
        // using its handle after any completion-time validation failure.
        Retiring->bCreationFailed = true;
    }
    ReplacementInFlight_ = {};
    bReplacementCreationFailed_ = true;
    return Failure;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::CompletePendingReplacement(
    uint32 ActualImageCount) noexcept
{
    if (!bReplacementInFlight_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }

    FVulkanLabPresentationGenerationDesc ActualDesc = ReplacementInFlight_;
    ActualDesc.ImageCount = ActualImageCount;
    if (!IsValidGenerationDesc(ActualDesc))
    {
        return FailPendingReplacementCreation(
            EVulkanLabPresentationPolicyResult::Invalid);
    }

    uint64 EstimatedBytes = 0;
    if (!TryEstimateColorBytes(ActualDesc, EstimatedBytes))
    {
        return FailPendingReplacementCreation(
            EVulkanLabPresentationPolicyResult::BudgetExceeded);
    }

    const FGenerationRecord* Retiring = FindRetiringGeneration();
    if (Retiring == nullptr ||
        Retiring->EstimatedColorBytes >
            MaxEstimatedColorBytes - EstimatedBytes)
    {
        return FailPendingReplacementCreation(
            EVulkanLabPresentationPolicyResult::BudgetExceeded);
    }

    for (FGenerationRecord& Record : Generations_)
    {
        if (!Record.bOccupied)
        {
            bReplacementInFlight_ = false;
            Record = {};
            Record.bOccupied = true;
            Record.bActive = true;
            Record.Desc = ActualDesc;
            Record.EstimatedColorBytes = EstimatedBytes;
            ReplacementInFlight_ = {};
            return EVulkanLabPresentationPolicyResult::Accepted;
        }
    }
    return FailPendingReplacementCreation(
        EVulkanLabPresentationPolicyResult::GenerationLimit);
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::CompletePendingReplacement(bool bCreated) noexcept
{
    if (!bCreated)
    {
        return FailPendingReplacementCreation(
            EVulkanLabPresentationPolicyResult::CreationFailed);
    }
    if (!bReplacementInFlight_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    // Preserve the deterministic policy-test API: a boolean success means
    // the requested count was also the actual count.  Native callers should
    // use the uint32 overload after vkGetSwapchainImagesKHR instead.
    return CompletePendingReplacement(ReplacementInFlight_.ImageCount);
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::RecordAcquisition(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) noexcept
{
    if (bTerminalCleanupStarted_)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    const FGenerationRecord* Active = FindActiveGeneration();
    if (Active == nullptr || Active->Desc.Generation != Generation ||
        AcquisitionToken == 0 || ImageIndex >= Active->Desc.ImageCount)
    {
        return EVulkanLabPresentationPolicyResult::Invalid;
    }
    if (FindPresentationRecord(Generation, ImageIndex, AcquisitionToken) != nullptr)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }

    for (FPresentationRecord& Existing : Records_)
    {
        if (!Existing.bOccupied || Existing.Generation != Generation ||
            Existing.ImageIndex != ImageIndex)
        {
            continue;
        }
        if (Existing.bCanceled || !Existing.bPresented)
        {
            return EVulkanLabPresentationPolicyResult::ImageOwnershipPending;
        }
        if (Existing.ReacquisitionToken != 0)
        {
            return EVulkanLabPresentationPolicyResult::ImageOwnershipPending;
        }
        // A second acquisition of an image is admitted as a proof candidate;
        // only completion of its acquire synchronization may retire Existing.
        // Keep the pending token in this per-image record so a full 8+8
        // active/retiring set never needs a seventeenth record.
        Existing.bReacquisitionObserved = !Existing.bPresentationRetired;
        Existing.ReacquisitionToken = AcquisitionToken;
        return EVulkanLabPresentationPolicyResult::Accepted;
    }

    for (FPresentationRecord& Record : Records_)
    {
        if (!Record.bOccupied)
        {
            Record = {};
            Record.bOccupied = true;
            Record.Generation = Generation;
            Record.ImageIndex = ImageIndex;
            Record.AcquisitionToken = AcquisitionToken;
            return EVulkanLabPresentationPolicyResult::Accepted;
        }
    }
    return EVulkanLabPresentationPolicyResult::GenerationLimit;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::RecordPresentationQueued(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) noexcept
{
    FPresentationRecord* Record =
        FindPresentationRecord(Generation, ImageIndex, AcquisitionToken);
    FGenerationRecord* GenerationRecord = FindGeneration(Generation);
    if (Record == nullptr || GenerationRecord == nullptr ||
        Record->bCanceled ||
        (IsReacquisitionRecord(*Record, AcquisitionToken)
            ? (Record->bReacquisitionPresented ||
               Record->bReacquisitionCanceled)
            : Record->bPresented))
    {
        return EVulkanLabPresentationPolicyResult::Invalid;
    }

    if (IsReacquisitionRecord(*Record, AcquisitionToken))
    {
        Record->bReacquisitionPresented = true;
        Record->bReacquisitionRenderSubmitted = true;
    }
    else
    {
        Record->bPresented = true;
        Record->bRenderSubmitted = true;
    }
    if (GenerationRecord->FirstPresentationToken == 0)
    {
        GenerationRecord->FirstPresentationToken = AcquisitionToken;
    }
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::MarkRenderSubmitted(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) noexcept
{
    FPresentationRecord* Record =
        FindPresentationRecord(Generation, ImageIndex, AcquisitionToken);
    if (Record == nullptr)
    {
        return EVulkanLabPresentationPolicyResult::NotFound;
    }
    if (IsReacquisitionRecord(*Record, AcquisitionToken))
    {
        if (Record->bReacquisitionCanceled)
        {
            return EVulkanLabPresentationPolicyResult::Invalid;
        }
        Record->bReacquisitionRenderSubmitted = true;
    }
    else
    {
        if (Record->bCanceled)
        {
            return EVulkanLabPresentationPolicyResult::Invalid;
        }
        Record->bRenderSubmitted = true;
    }
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::MarkRenderComplete(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) noexcept
{
    FPresentationRecord* Record =
        FindPresentationRecord(Generation, ImageIndex, AcquisitionToken);
    if (Record == nullptr)
    {
        return EVulkanLabPresentationPolicyResult::NotFound;
    }
    if (IsReacquisitionRecord(*Record, AcquisitionToken))
    {
        if (Record->bReacquisitionCanceled)
        {
            return EVulkanLabPresentationPolicyResult::Invalid;
        }
        Record->bReacquisitionRenderComplete = true;
        PromoteReacquisitionIfReady(*Record);
        return EVulkanLabPresentationPolicyResult::Accepted;
    }
    Record->bRenderComplete = true;
    ReleaseRecordIfComplete(*Record);
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::MarkPresentationFenceComplete(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) noexcept
{
    if (Mode_ != ERHIPresentationRetirementMode::PresentationFence)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    FPresentationRecord* Record =
        FindPresentationRecord(Generation, ImageIndex, AcquisitionToken);
    FGenerationRecord* GenerationRecord = FindGeneration(Generation);
    if (Record == nullptr || GenerationRecord == nullptr ||
        IsReacquisitionRecord(*Record, AcquisitionToken) ||
        !Record->bPresented || Record->bCanceled)
    {
        return EVulkanLabPresentationPolicyResult::Invalid;
    }
    Record->bPresentationRetired = true;
    if (GenerationRecord->FirstPresentationToken == AcquisitionToken)
    {
        GenerationRecord->bFirstPresentationRetired = true;
    }
    ReleaseRecordIfComplete(*Record);
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::MarkAcquireSynchronizationComplete(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 ReacquisitionToken) noexcept
{
    if (Mode_ != ERHIPresentationRetirementMode::AcquireHistory)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    FPresentationRecord* Reacquisition =
        FindPresentationRecord(Generation, ImageIndex, ReacquisitionToken);
    FGenerationRecord* GenerationRecord = FindGeneration(Generation);
    const bool bPendingReacquisition =
        Reacquisition != nullptr &&
        IsReacquisitionRecord(*Reacquisition, ReacquisitionToken);
    if (Reacquisition == nullptr || GenerationRecord == nullptr ||
        Reacquisition->bCanceled ||
        (bPendingReacquisition && Reacquisition->bReacquisitionCanceled))
    {
        return EVulkanLabPresentationPolicyResult::Invalid;
    }

    if (bPendingReacquisition && Reacquisition->bReacquisitionObserved &&
        Reacquisition->bPresented && !Reacquisition->bCanceled)
    {
        Reacquisition->bPresentationRetired = true;
        if (GenerationRecord->FirstPresentationToken ==
            Reacquisition->AcquisitionToken)
        {
            GenerationRecord->bFirstPresentationRetired = true;
        }
    }

    for (FPresentationRecord& Previous : Records_)
    {
        if (&Previous == Reacquisition || !Previous.bOccupied ||
            Previous.Generation != Generation ||
            Previous.ImageIndex != ImageIndex ||
            !Previous.bReacquisitionObserved || !Previous.bPresented ||
            Previous.bCanceled)
        {
            continue;
        }
        Previous.bPresentationRetired = true;
        if (GenerationRecord->FirstPresentationToken ==
            Previous.AcquisitionToken)
        {
            GenerationRecord->bFirstPresentationRetired = true;
        }
        ReleaseRecordIfComplete(Previous);
    }

    if (bPendingReacquisition)
    {
        Reacquisition->bReacquisitionSynchronized = true;
        // The old render state remains in the same record until it has
        // completed; promotion then preserves the current acquisition state.
        PromoteReacquisitionIfReady(*Reacquisition);
    }

    // The reacquired image is still owned by this current acquisition.  Its
    // acquire synchronization is a proof for the predecessor only.
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::MarkRetiringGenerationRenderUsesComplete(
    uint64 Generation) noexcept
{
    FGenerationRecord* Retiring = FindRetiringGeneration();
    if (Retiring == nullptr || Retiring->Desc.Generation != Generation)
    {
        return EVulkanLabPresentationPolicyResult::NotFound;
    }

    for (const FPresentationRecord& Record : Records_)
    {
        if (Record.bOccupied && Record.Generation == Generation &&
            (!Record.bCanceled || Record.bRenderSubmitted) &&
            !Record.bRenderComplete)
        {
            return EVulkanLabPresentationPolicyResult::ImageOwnershipPending;
        }
    }
    // Canceled/unpresented records remain retained until RetirePredecessor's
    // generation teardown clears the old native swapchain ownership.
    Retiring->bRenderUsesDrained = true;
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::CancelAcquisition(
    uint64 Generation,
    uint32 ImageIndex,
    uint64 AcquisitionToken) noexcept
{
    FPresentationRecord* Record =
        FindPresentationRecord(Generation, ImageIndex, AcquisitionToken);
    if (Record == nullptr)
    {
        return EVulkanLabPresentationPolicyResult::Invalid;
    }
    // Cancellation does not return a Vulkan image to the presentation engine;
    // retain this record until a generation teardown or terminal cleanup.
    if (IsReacquisitionRecord(*Record, AcquisitionToken))
    {
        if (Record->bReacquisitionPresented || Record->bReacquisitionCanceled)
        {
            return EVulkanLabPresentationPolicyResult::Invalid;
        }
        Record->bReacquisitionCanceled = true;
    }
    else
    {
        if (Record->bPresented || Record->bCanceled)
        {
            return EVulkanLabPresentationPolicyResult::Invalid;
        }
        Record->bCanceled = true;
    }
    return EVulkanLabPresentationPolicyResult::Accepted;
}

bool FVulkanLabPresentationPolicy::IsImageReusable(
    uint64 Generation,
    uint32 ImageIndex) const noexcept
{
    return FindGeneration(Generation) != nullptr &&
        !HasOutstandingImageRecord(Generation, ImageIndex);
}

bool FVulkanLabPresentationPolicy::CanRetirePredecessor() const noexcept
{
    const FGenerationRecord* Retiring = FindRetiringGeneration();
    const FGenerationRecord* Active = FindActiveGeneration();
    if (Retiring == nullptr || Active == nullptr ||
        !Active->bFirstPresentationRetired || !Retiring->bRenderUsesDrained)
    {
        return false;
    }
    return true;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::RetirePredecessor() noexcept
{
    if (!CanRetirePredecessor())
    {
        return EVulkanLabPresentationPolicyResult::ImageOwnershipPending;
    }
    if (FGenerationRecord* Retiring = FindRetiringGeneration())
    {
        const uint64 RetiringGeneration = Retiring->Desc.Generation;
        for (FPresentationRecord& Record : Records_)
        {
            if (Record.bOccupied && Record.Generation == RetiringGeneration)
            {
                Record = {};
            }
        }
        *Retiring = {};
        return EVulkanLabPresentationPolicyResult::Accepted;
    }
    return EVulkanLabPresentationPolicyResult::StateConflict;
}

FVulkanLabPresentationGenerationView
FVulkanLabPresentationPolicy::GetActiveGeneration() const noexcept
{
    FVulkanLabPresentationGenerationView Result;
    const FGenerationRecord* Record = FindActiveGeneration();
    if (Record == nullptr)
    {
        return Result;
    }
    Result.bValid = true;
    Result.bActive = true;
    Result.bCreationFailed = Record->bCreationFailed;
    Result.bFirstPresentationRetired = Record->bFirstPresentationRetired;
    Result.EstimatedColorBytes = Record->EstimatedColorBytes;
    Result.Desc = Record->Desc;
    for (const FPresentationRecord& Presentation : Records_)
    {
        if (Presentation.bOccupied &&
            Presentation.Generation == Record->Desc.Generation)
        {
            ++Result.OutstandingRecordCount;
        }
    }
    return Result;
}

FVulkanLabPresentationGenerationView
FVulkanLabPresentationPolicy::GetRetiringGeneration() const noexcept
{
    FVulkanLabPresentationGenerationView Result;
    const FGenerationRecord* Record = FindRetiringGeneration();
    if (Record == nullptr)
    {
        return Result;
    }
    Result.bValid = true;
    Result.bRetiring = true;
    Result.bCreationFailed = Record->bCreationFailed;
    Result.bFirstPresentationRetired = Record->bFirstPresentationRetired;
    Result.EstimatedColorBytes = Record->EstimatedColorBytes;
    Result.Desc = Record->Desc;
    for (const FPresentationRecord& Presentation : Records_)
    {
        if (Presentation.bOccupied &&
            Presentation.Generation == Record->Desc.Generation)
        {
            ++Result.OutstandingRecordCount;
        }
    }
    return Result;
}

uint32 FVulkanLabPresentationPolicy::GetOutstandingRecordCount() const noexcept
{
    uint32 Count = 0;
    for (const FPresentationRecord& Record : Records_)
    {
        if (Record.bOccupied)
        {
            ++Count;
        }
    }
    return Count;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::ResolveTerminalOwnersForCompatibility() noexcept
{
    if (!bTerminalCleanupStarted_ ||
        Mode_ != ERHIPresentationRetirementMode::AcquireHistory ||
        !bTerminalIdleSucceeded_ ||
        ShutdownAssurance_ == Stoner::RHI::ERHIShutdownAssurance::Forced ||
        ShutdownAssurance_ == Stoner::RHI::ERHIShutdownAssurance::DeviceLost)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }
    for (FPresentationRecord& Record : Records_)
    {
        Record = {};
    }
    for (FGenerationRecord& Record : Generations_)
    {
        Record = {};
    }
    PendingReplacement_ = {};
    ReplacementInFlight_ = {};
    bHasPendingReplacement_ = false;
    bReplacementInFlight_ = false;
    return EVulkanLabPresentationPolicyResult::Accepted;
}

EVulkanLabPresentationPolicyResult
FVulkanLabPresentationPolicy::CompleteTerminalIdle(bool bIdleSucceeded) noexcept
{
    if (!bTerminalCleanupStarted_ || !bIdleSucceeded ||
        ShutdownAssurance_ == Stoner::RHI::ERHIShutdownAssurance::Forced ||
        ShutdownAssurance_ == Stoner::RHI::ERHIShutdownAssurance::DeviceLost)
    {
        return EVulkanLabPresentationPolicyResult::StateConflict;
    }

    if (Mode_ == ERHIPresentationRetirementMode::AcquireHistory)
    {
        bTerminalIdleSucceeded_ = true;
        ShutdownAssurance_ =
            Stoner::RHI::ERHIShutdownAssurance::IdleAssumed;
    }
    else if ((Mode_ == ERHIPresentationRetirementMode::PresentationFence ||
              Mode_ == ERHIPresentationRetirementMode::NativeCallback) &&
             GetOutstandingRecordCount() == 0)
    {
        ShutdownAssurance_ = Stoner::RHI::ERHIShutdownAssurance::Proven;
    }
    else
    {
        return EVulkanLabPresentationPolicyResult::ImageOwnershipPending;
    }
    return EVulkanLabPresentationPolicyResult::Accepted;
}

void FVulkanLabPresentationPolicy::MarkForcedTermination() noexcept
{
    ShutdownAssurance_ = Stoner::RHI::ERHIShutdownAssurance::Forced;
}

void FVulkanLabPresentationPolicy::MarkDeviceLost() noexcept
{
    ShutdownAssurance_ = Stoner::RHI::ERHIShutdownAssurance::DeviceLost;
}

} // namespace Stoner::Backend::Vulkan::Private
