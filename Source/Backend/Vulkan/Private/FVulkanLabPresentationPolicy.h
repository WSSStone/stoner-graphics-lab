#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationRetirement.h"
#include "RHI/ERHIResult.h"

#include <array>

namespace Stoner::Backend::Vulkan::Private
{

enum class EVulkanLabCapabilityQueryState
{
    NotAttempted,
    Succeeded,
    Unavailable,
    Failed
};

enum class EVulkanLabOptionalEnablementState
{
    NotAttempted,
    Succeeded,
    Failed
};

enum class EVulkanLabFallbackDeviceCreationState
{
    NotAttempted,
    Succeeded,
    Failed
};

enum class EVulkanLabPresentationCandidate
{
    None,
    PresentationFence,
    AcquireHistory
};

// Results are private to the Vulkan policy helper.  Native call sites can
// translate them to ERHIResult while retaining the more useful diagnostic
// distinction for tests and reports.
enum class EVulkanLabPresentationPolicyResult
{
    Accepted,
    Coalesced,
    Invalid,
    BudgetExceeded,
    GenerationLimit,
    NoActiveGeneration,
    NoPendingReplacement,
    StateConflict,
    ImageOwnershipPending,
    NotFound,
    CreationFailed
};

struct FVulkanLabPresentationCapabilityObservation
{
    EVulkanLabCapabilityQueryState QueryState =
        EVulkanLabCapabilityQueryState::NotAttempted;
    bool bExtensionAdvertised = false;
    bool bFeatureAdvertised = false;
    bool bInstanceDependenciesAvailable = false;
    bool bEntryPointsResolved = false;
    Stoner::Core::int32 NativeQueryResult = 0;
    Stoner::Core::FString QueryFailure;
};

struct FVulkanLabPresentationPolicyRequest
{
    // Force-off is applied before device creation and therefore does not
    // require querying optional feature support.
    bool bForceAcquireHistory = false;
    bool bAllowOptionalPresentationFence = true;
};

struct FVulkanLabPresentationPolicyCandidate
{
    EVulkanLabPresentationCandidate Candidate =
        EVulkanLabPresentationCandidate::None;
    Stoner::RHI::ERHIPresentationRetirementReason Reason =
        Stoner::RHI::ERHIPresentationRetirementReason::Unknown;
    bool bRequiresOptionalEnablement = false;
    bool bExtensionAdvertised = false;
    bool bFeatureAdvertised = false;
    bool bInstanceDependenciesAvailable = false;
    bool bEntryPointsResolved = false;
    Stoner::Core::int32 NativeQueryResult = 0;
    Stoner::Core::FString QueryFailure;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Candidate != EVulkanLabPresentationCandidate::None;
    }
};

struct FVulkanLabPresentationPolicySelection
{
    Stoner::RHI::ERHIPresentationRetirementMode Mode =
        Stoner::RHI::ERHIPresentationRetirementMode::Unknown;
    Stoner::RHI::ERHIPresentationRetirementReason Reason =
        Stoner::RHI::ERHIPresentationRetirementReason::Unknown;
    bool bRetryWithoutOptional = false;
    bool bExtensionAdvertised = false;
    bool bFeatureAdvertised = false;
    bool bInstanceDependenciesAvailable = false;
    bool bEntryPointsResolved = false;
    Stoner::Core::int32 NativeQueryResult = 0;
    Stoner::Core::FString QueryFailure;
    bool bOptionalEnabled = false;
    bool bRetryPending = false;

    [[nodiscard]] bool IsUsable() const noexcept
    {
        return Mode != Stoner::RHI::ERHIPresentationRetirementMode::Unknown &&
            !bRetryPending;
    }
};

struct FVulkanLabPresentationGenerationDesc
{
    Stoner::Core::uint64 Generation = 0;
    Stoner::Core::uint32 ImageCount = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::RHI::ERHIFormat ColorFormat =
        Stoner::RHI::ERHIFormat::Unknown;
};

struct FVulkanLabPresentationGenerationView
{
    bool bValid = false;
    bool bActive = false;
    bool bRetiring = false;
    bool bCreationFailed = false;
    bool bFirstPresentationRetired = false;
    Stoner::Core::uint64 EstimatedColorBytes = 0;
    Stoner::Core::uint32 OutstandingRecordCount = 0;
    FVulkanLabPresentationGenerationDesc Desc;
};

class FVulkanLabPresentationPolicy final
{
public:
    static constexpr Stoner::Core::uint32 MaxImagesPerGeneration = 8;
    static constexpr Stoner::Core::uint32 MaxGenerationCount = 2;
    static constexpr Stoner::Core::uint32 MaxPresentationRecords = 16;
    static constexpr Stoner::Core::uint64 MaxEstimatedColorBytes =
        512ull * 1024ull * 1024ull;

    [[nodiscard]] static FVulkanLabPresentationPolicyCandidate
    SelectCandidate(
        const FVulkanLabPresentationPolicyRequest& Request,
        const FVulkanLabPresentationCapabilityObservation& Observation)
        noexcept;

    [[nodiscard]] static FVulkanLabPresentationPolicySelection
    CompleteEnablement(
        const FVulkanLabPresentationPolicyCandidate& Candidate,
        EVulkanLabOptionalEnablementState Enablement,
        bool bUsedEntryPointsResolved,
        bool bFallbackRetryAlreadyConsumed) noexcept;

    // A fallback selection returned after optional device creation failure is
    // only a retry request.  The native caller must report ordinary device
    // creation before it can publish AcquireHistory.
    [[nodiscard]] static FVulkanLabPresentationPolicySelection
    CompleteFallbackRetry(
        const FVulkanLabPresentationPolicySelection& Retry,
        EVulkanLabFallbackDeviceCreationState DeviceCreation) noexcept;

    explicit FVulkanLabPresentationPolicy(
        Stoner::RHI::ERHIPresentationRetirementMode Mode =
            Stoner::RHI::ERHIPresentationRetirementMode::Unknown) noexcept;

    [[nodiscard]] Stoner::RHI::ERHIPresentationRetirementMode
    GetMode() const noexcept
    {
        return Mode_;
    }

    [[nodiscard]] EVulkanLabPresentationPolicyResult AdmitInitialGeneration(
        const FVulkanLabPresentationGenerationDesc& Desc) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult QueueReplacement(
        const FVulkanLabPresentationGenerationDesc& Desc) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    BeginPendingReplacement() noexcept;
    // The native caller supplies the image count returned by
    // vkGetSwapchainImagesKHR.  This count may differ from the requested
    // minimum recorded in the in-flight descriptor and must be validated
    // before the replacement becomes active.
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    CompletePendingReplacement(Stoner::Core::uint32 ActualImageCount) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    CompletePendingReplacement(bool bCreated) noexcept;

    [[nodiscard]] EVulkanLabPresentationPolicyResult RecordAcquisition(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    RecordPresentationQueued(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult MarkRenderSubmitted(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult MarkRenderComplete(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    MarkPresentationFenceComplete(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    MarkAcquireSynchronizationComplete(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 ReacquisitionToken) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    MarkRetiringGenerationRenderUsesComplete(
        Stoner::Core::uint64 Generation) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult CancelAcquisition(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) noexcept;

    [[nodiscard]] bool IsImageReusable(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex) const noexcept;
    [[nodiscard]] bool CanRetirePredecessor() const noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    RetirePredecessor() noexcept;

    [[nodiscard]] FVulkanLabPresentationGenerationView
    GetActiveGeneration() const noexcept;
    [[nodiscard]] FVulkanLabPresentationGenerationView
    GetRetiringGeneration() const noexcept;
    [[nodiscard]] bool HasPendingReplacement() const noexcept
    {
        return bHasPendingReplacement_;
    }
    [[nodiscard]] bool WasReplacementCreationFailed() const noexcept
    {
        return bReplacementCreationFailed_;
    }
    [[nodiscard]] Stoner::Core::uint32 GetOutstandingRecordCount() const noexcept;
    [[nodiscard]] Stoner::Core::uint32
    GetTerminalOutstandingRecordCount() const noexcept
    {
        return TerminalOutstandingRecordCount_;
    }

    void BeginTerminalCleanup() noexcept
    {
        bTerminalCleanupStarted_ = true;
        bTerminalIdleSucceeded_ = false;
        TerminalOutstandingRecordCount_ = GetOutstandingRecordCount();
    }
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    ResolveTerminalOwnersForCompatibility() noexcept;
    // The terminal caller must have independently proved that every native
    // acquire has completed and that each submitted render use is complete.
    // This clears only non-presented owners at terminal cleanup; queued or
    // unretired presentations and reacquisitions whose predecessor proof is
    // incomplete remain retained. It never changes a record into presented
    // state and never authorizes cleanup after forced/device-lost failure.
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    ResolveTerminalNonPresentedOwners() noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    CompleteTerminalIdle(bool bIdleSucceeded) noexcept;
    void MarkForcedTermination() noexcept;
    void MarkDeviceLost() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIShutdownAssurance
    GetShutdownAssurance() const noexcept
    {
        return ShutdownAssurance_;
    }

private:
    struct FGenerationRecord
    {
        bool bOccupied = false;
        bool bActive = false;
        bool bRetiring = false;
        bool bCreationFailed = false;
        bool bFirstPresentationRetired = false;
        bool bRenderUsesDrained = false;
        Stoner::Core::uint64 FirstPresentationToken = 0;
        FVulkanLabPresentationGenerationDesc Desc;
        Stoner::Core::uint64 EstimatedColorBytes = 0;
    };

    struct FPresentationRecord
    {
        bool bOccupied = false;
        bool bPresented = false;
        bool bCanceled = false;
        bool bRenderSubmitted = false;
        bool bRenderComplete = false;
        bool bPresentationRetired = false;
        bool bReacquisitionObserved = false;
        bool bReacquisitionSynchronized = false;
        bool bReacquisitionPresented = false;
        bool bReacquisitionCanceled = false;
        bool bReacquisitionRenderSubmitted = false;
        bool bReacquisitionRenderComplete = false;
        Stoner::Core::uint64 Generation = 0;
        Stoner::Core::uint32 ImageIndex = 0;
        Stoner::Core::uint64 AcquisitionToken = 0;
        Stoner::Core::uint64 ReacquisitionToken = 0;
    };

    [[nodiscard]] static bool TryEstimateColorBytes(
        const FVulkanLabPresentationGenerationDesc& Desc,
        Stoner::Core::uint64& OutBytes) noexcept;
    [[nodiscard]] static bool IsValidGenerationDesc(
        const FVulkanLabPresentationGenerationDesc& Desc) noexcept;
    [[nodiscard]] FGenerationRecord* FindGeneration(
        Stoner::Core::uint64 Generation) noexcept;
    [[nodiscard]] const FGenerationRecord* FindGeneration(
        Stoner::Core::uint64 Generation) const noexcept;
    [[nodiscard]] FGenerationRecord* FindActiveGeneration() noexcept;
    [[nodiscard]] const FGenerationRecord* FindActiveGeneration() const noexcept;
    [[nodiscard]] FGenerationRecord* FindRetiringGeneration() noexcept;
    [[nodiscard]] const FGenerationRecord* FindRetiringGeneration() const noexcept;
    [[nodiscard]] FPresentationRecord* FindPresentationRecord(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] const FPresentationRecord* FindPresentationRecord(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 AcquisitionToken) const noexcept;
    [[nodiscard]] bool IsReacquisitionRecord(
        const FPresentationRecord& Record,
        Stoner::Core::uint64 AcquisitionToken) const noexcept;
    void PromoteReacquisitionIfReady(FPresentationRecord& Record) noexcept;
    [[nodiscard]] bool HasOutstandingImageRecord(
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex) const noexcept;
    [[nodiscard]] bool CanReleaseRecord(
        const FPresentationRecord& Record) const noexcept;
    void ReleaseRecordIfComplete(FPresentationRecord& Record) noexcept;
    [[nodiscard]] EVulkanLabPresentationPolicyResult
    FailPendingReplacementCreation(
        EVulkanLabPresentationPolicyResult Failure) noexcept;

    Stoner::RHI::ERHIPresentationRetirementMode Mode_ =
        Stoner::RHI::ERHIPresentationRetirementMode::Unknown;
    std::array<FGenerationRecord, MaxGenerationCount> Generations_{};
    std::array<FPresentationRecord, MaxPresentationRecords> Records_{};
    FVulkanLabPresentationGenerationDesc PendingReplacement_;
    FVulkanLabPresentationGenerationDesc ReplacementInFlight_;
    bool bHasPendingReplacement_ = false;
    bool bReplacementInFlight_ = false;
    bool bReplacementCreationFailed_ = false;
    bool bTerminalCleanupStarted_ = false;
    bool bTerminalIdleSucceeded_ = false;
    Stoner::Core::uint32 TerminalOutstandingRecordCount_ = 0;
    Stoner::RHI::ERHIShutdownAssurance ShutdownAssurance_ =
        Stoner::RHI::ERHIShutdownAssurance::Unknown;
};

} // namespace Stoner::Backend::Vulkan::Private
