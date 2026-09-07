#include "FVulkanLabPresentationPolicy.h"

#include "RHI/FRHIPresentationCapabilities.h"

#include <iostream>

namespace
{

using namespace Stoner::Backend::Vulkan::Private;
using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FTestState
{
    int Failed = 0;
};

void Check(FTestState& State, bool bPassed, const char* Name)
{
    if (!bPassed)
    {
        ++State.Failed;
    }
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FVulkanLabPresentationCapabilityObservation AvailableObservation()
{
    FVulkanLabPresentationCapabilityObservation Observation;
    Observation.QueryState = EVulkanLabCapabilityQueryState::Succeeded;
    Observation.bExtensionAdvertised = true;
    Observation.bFeatureAdvertised = true;
    Observation.bInstanceDependenciesAvailable = true;
    Observation.bEntryPointsResolved = true;
    return Observation;
}

FVulkanLabPresentationGenerationDesc MakeGeneration(
    uint64 Generation,
    uint32 ImageCount = 2,
    uint32 Width = 640,
    uint32 Height = 360,
    ERHIFormat Format = ERHIFormat::R8G8B8A8_UNorm)
{
    FVulkanLabPresentationGenerationDesc Desc;
    Desc.Generation = Generation;
    Desc.ImageCount = ImageCount;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.ColorFormat = Format;
    return Desc;
}

void TestCapabilitySelection(FTestState& State)
{
    const auto Available = AvailableObservation();
    const auto Candidate = FVulkanLabPresentationPolicy::SelectCandidate(
        {}, Available);
    Check(State,
        Candidate.Candidate == EVulkanLabPresentationCandidate::PresentationFence &&
            Candidate.bRequiresOptionalEnablement &&
            Candidate.Reason == ERHIPresentationRetirementReason::Preferred,
        "advertised feature and resolved entrypoints produce a fence candidate");

    const auto BeforeEnablement =
        FVulkanLabPresentationPolicy::CompleteEnablement(
            Candidate,
            EVulkanLabOptionalEnablementState::NotAttempted,
            false,
            false);
    Check(State,
        BeforeEnablement.Mode == ERHIPresentationRetirementMode::Unknown,
        "advertisement alone cannot publish PresentationFence");

    const auto Enabled = FVulkanLabPresentationPolicy::CompleteEnablement(
        Candidate,
        EVulkanLabOptionalEnablementState::Succeeded,
        true,
        false);
    Check(State,
        Enabled.Mode == ERHIPresentationRetirementMode::PresentationFence &&
            Enabled.bOptionalEnabled &&
            Enabled.Reason == ERHIPresentationRetirementReason::Preferred,
        "successful optional enablement publishes PresentationFence");

    auto Missing = Available;
    Missing.bExtensionAdvertised = false;
    auto MissingCandidate = FVulkanLabPresentationPolicy::SelectCandidate(
        {}, Missing);
    Check(State,
        MissingCandidate.Candidate == EVulkanLabPresentationCandidate::AcquireHistory &&
            MissingCandidate.Reason == ERHIPresentationRetirementReason::ExtensionAbsent,
        "missing extension selects AcquireHistory");

    Missing = Available;
    Missing.bFeatureAdvertised = false;
    MissingCandidate = FVulkanLabPresentationPolicy::SelectCandidate({}, Missing);
    Check(State,
        MissingCandidate.Reason == ERHIPresentationRetirementReason::FeatureAbsent,
        "advertised extension with false feature selects AcquireHistory");

    Missing = Available;
    Missing.bInstanceDependenciesAvailable = false;
    MissingCandidate = FVulkanLabPresentationPolicy::SelectCandidate({}, Missing);
    Check(State,
        MissingCandidate.Reason ==
            ERHIPresentationRetirementReason::InstanceDependencyAbsent,
        "missing instance dependency cannot enable maintenance1");

    Missing = Available;
    Missing.bEntryPointsResolved = false;
    MissingCandidate = FVulkanLabPresentationPolicy::SelectCandidate({}, Missing);
    Check(State,
        MissingCandidate.Candidate ==
                EVulkanLabPresentationCandidate::PresentationFence &&
            MissingCandidate.bRequiresOptionalEnablement,
        "post-create entrypoint resolution does not block the candidate");
    const auto MissingEntryPoints =
        FVulkanLabPresentationPolicy::CompleteEnablement(
            MissingCandidate,
            EVulkanLabOptionalEnablementState::Succeeded,
            false,
            false);
    Check(State,
        MissingEntryPoints.Mode == ERHIPresentationRetirementMode::AcquireHistory &&
            MissingEntryPoints.bRetryWithoutOptional &&
            MissingEntryPoints.Reason ==
                ERHIPresentationRetirementReason::EntryPointsUnavailable,
        "missing used entrypoints falls back before submission");
    const auto ResolvedAfterCreation =
        FVulkanLabPresentationPolicy::CompleteEnablement(
            MissingCandidate, EVulkanLabOptionalEnablementState::Succeeded,
            true, false);
    Check(State, ResolvedAfterCreation.bEntryPointsResolved &&
            ResolvedAfterCreation.IsUsable(),
        "published entrypoint evidence reflects actual device creation");

    FVulkanLabPresentationPolicyRequest ForceOff;
    ForceOff.bForceAcquireHistory = true;
    const auto Forced = FVulkanLabPresentationPolicy::SelectCandidate(
        ForceOff,
        FVulkanLabPresentationCapabilityObservation{
            EVulkanLabCapabilityQueryState::Failed,
            true,
            true,
            true,
            true,
            -13,
            "query failed"});
    Check(State,
        Forced.Candidate == EVulkanLabPresentationCandidate::AcquireHistory &&
            Forced.Reason == ERHIPresentationRetirementReason::ForcedOff,
        "force-off bypasses optional query and selects fallback");

    auto QueryUnavailable = Available;
    QueryUnavailable.QueryState = EVulkanLabCapabilityQueryState::Unavailable;
    const auto QueryUnavailableCandidate =
        FVulkanLabPresentationPolicy::SelectCandidate({}, QueryUnavailable);
    Check(State,
        QueryUnavailableCandidate.Candidate ==
                EVulkanLabPresentationCandidate::AcquireHistory &&
            QueryUnavailableCandidate.Reason ==
                ERHIPresentationRetirementReason::QueryUnavailable,
        "unavailable optional query is distinct from a failed query");

    auto QueryFailed = Available;
    QueryFailed.QueryState = EVulkanLabCapabilityQueryState::Failed;
    QueryFailed.NativeQueryResult = -100001;
    QueryFailed.QueryFailure = "vkGetPhysicalDeviceFeatures2 failed";
    const auto QueryFailedCandidate =
        FVulkanLabPresentationPolicy::SelectCandidate({}, QueryFailed);
    Check(State,
        QueryFailedCandidate.Candidate == EVulkanLabPresentationCandidate::None &&
            QueryFailedCandidate.Reason ==
                ERHIPresentationRetirementReason::QueryFailed &&
            QueryFailedCandidate.NativeQueryResult == -100001 &&
            QueryFailedCandidate.QueryFailure ==
                "vkGetPhysicalDeviceFeatures2 failed",
        "native capability query failure retains its error evidence");
    const auto FailedSelection = FVulkanLabPresentationPolicy::CompleteEnablement(
        QueryFailedCandidate, EVulkanLabOptionalEnablementState::NotAttempted,
        false, false);
    Check(State, !FailedSelection.IsUsable() &&
            FailedSelection.Reason == ERHIPresentationRetirementReason::QueryFailed &&
            FailedSelection.NativeQueryResult == QueryFailed.NativeQueryResult &&
            FailedSelection.QueryFailure == QueryFailed.QueryFailure,
        "failed final selection preserves the original query failure");

    const auto Retried = FVulkanLabPresentationPolicy::CompleteEnablement(
        Candidate,
        EVulkanLabOptionalEnablementState::Failed,
        true,
        false);
    Check(State,
        Retried.Mode == ERHIPresentationRetirementMode::AcquireHistory &&
            Retried.bRetryWithoutOptional &&
            Retried.bRetryPending &&
            !Retried.IsUsable() &&
            Retried.Reason ==
                ERHIPresentationRetirementReason::OptionalEnablementFailed,
        "optional enablement failure leaves one fallback retry pending");

    Check(State,
        FVulkanLabPresentationPolicy::CompleteFallbackRetry(
            Retried,
            EVulkanLabFallbackDeviceCreationState::NotAttempted)
                .bRetryPending,
        "fallback retry cannot finalize before baseline device creation");
    const auto FallbackFailed =
        FVulkanLabPresentationPolicy::CompleteFallbackRetry(
            Retried,
            EVulkanLabFallbackDeviceCreationState::Failed);
    Check(State,
        FallbackFailed.Mode == ERHIPresentationRetirementMode::Unknown &&
            !FallbackFailed.IsUsable() &&
            FallbackFailed.Reason ==
                ERHIPresentationRetirementReason::FallbackCreationFailed,
        "failed fallback device creation cannot publish AcquireHistory");
    const auto FallbackSucceeded =
        FVulkanLabPresentationPolicy::CompleteFallbackRetry(
            Retried,
            EVulkanLabFallbackDeviceCreationState::Succeeded);
    Check(State,
        FallbackSucceeded.Mode == ERHIPresentationRetirementMode::AcquireHistory &&
            FallbackSucceeded.IsUsable() &&
            !FallbackSucceeded.bRetryPending &&
            !FallbackSucceeded.bRetryWithoutOptional,
        "successful fallback device creation finalizes AcquireHistory");

    const auto Exhausted = FVulkanLabPresentationPolicy::CompleteEnablement(
        Candidate,
        EVulkanLabOptionalEnablementState::Failed,
        true,
        true);
    Check(State,
        Exhausted.Mode == ERHIPresentationRetirementMode::Unknown &&
            Exhausted.Reason ==
                ERHIPresentationRetirementReason::OptionalEnablementRetryExhausted &&
            !Exhausted.bRetryPending &&
            !Exhausted.bRetryWithoutOptional,
        "a second optional enablement failure cannot retry again");
}

void TestImageIndexedRetirement(FTestState& State)
{
    FVulkanLabPresentationPolicy Policy(
        ERHIPresentationRetirementMode::AcquireHistory);
    Check(State,
        Policy.AdmitInitialGeneration(MakeGeneration(1, 2)) ==
            EVulkanLabPresentationPolicyResult::Accepted,
        "initial generation is admitted");
    Check(State,
        Policy.RecordAcquisition(1, 0, 101) ==
            EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.RecordPresentationQueued(1, 0, 101) ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "acquired image is tracked before presentation");
    Check(State,
        Policy.MarkRenderComplete(1, 0, 101) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            !Policy.IsImageReusable(1, 0),
        "render completion alone does not release an image-indexed lease");
    Check(State,
        Policy.RecordAcquisition(1, 0, 102) ==
            EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.GetOutstandingRecordCount() == 1 &&
            !Policy.IsImageReusable(1, 0),
        "reacquisition is retained until its acquire synchronization completes");
    Check(State,
        Policy.MarkAcquireSynchronizationComplete(1, 0, 102) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.GetOutstandingRecordCount() == 1,
        "same-image acquire synchronization retires the prior presentation");

    FVulkanLabPresentationPolicy CanceledReacquisition(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)CanceledReacquisition.AdmitInitialGeneration(
        MakeGeneration(1, 1, 64, 64));
    (void)CanceledReacquisition.RecordAcquisition(1, 0, 301);
    (void)CanceledReacquisition.RecordPresentationQueued(1, 0, 301);
    (void)CanceledReacquisition.MarkRenderComplete(1, 0, 301);
    Check(State,
        CanceledReacquisition.RecordAcquisition(1, 0, 302) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledReacquisition.CancelAcquisition(1, 0, 302) ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "an acquired replacement image may be logically canceled before presentation");
    Check(State,
        CanceledReacquisition.MarkAcquireSynchronizationComplete(1, 0, 302) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledReacquisition.GetActiveGeneration().bFirstPresentationRetired &&
            CanceledReacquisition.RecordPresentationQueued(1, 0, 302) ==
                EVulkanLabPresentationPolicyResult::Invalid &&
            CanceledReacquisition.MarkRenderComplete(1, 0, 302) ==
                EVulkanLabPresentationPolicyResult::Invalid &&
            CanceledReacquisition.GetOutstandingRecordCount() == 1 &&
            !CanceledReacquisition.IsImageReusable(1, 0) &&
            CanceledReacquisition.RecordAcquisition(1, 0, 303) ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending,
        "canceled reacquisition still proves the prior presentation but remains owned");
    CanceledReacquisition.BeginTerminalCleanup();
    Check(State,
        CanceledReacquisition.GetTerminalOutstandingRecordCount() == 1 &&
            CanceledReacquisition.RecordAcquisition(1, 0, 304) ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            CanceledReacquisition.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledReacquisition.ResolveTerminalOwnersForCompatibility() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledReacquisition.GetOutstandingRecordCount() == 0,
        "canceled reacquisition stays retained until explicit terminal cleanup");

    Check(State,
        Policy.CancelAcquisition(1, 1, 201) ==
                EVulkanLabPresentationPolicyResult::Invalid,
        "unknown cancellation is rejected");
    Check(State,
        Policy.RecordAcquisition(1, 1, 201) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.CancelAcquisition(1, 1, 201) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.RecordAcquisition(1, 1, 202) ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending,
        "canceled unpresented image remains retained and cannot be recycled");

    FVulkanLabPresentationPolicy SubmittedCancel(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)SubmittedCancel.AdmitInitialGeneration(MakeGeneration(1, 1, 64, 64));
    (void)SubmittedCancel.RecordAcquisition(1, 0, 1);
    Check(State,
        SubmittedCancel.MarkRenderSubmitted(1, 0, 1) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            SubmittedCancel.CancelAcquisition(1, 0, 1) ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "cancellation after render submission retains its pending render use");
    (void)SubmittedCancel.QueueReplacement(MakeGeneration(2, 1, 64, 64));
    (void)SubmittedCancel.BeginPendingReplacement();
    (void)SubmittedCancel.CompletePendingReplacement(true);
    Check(State,
        SubmittedCancel.MarkRetiringGenerationRenderUsesComplete(1) ==
            EVulkanLabPresentationPolicyResult::ImageOwnershipPending,
        "submitted cancellation cannot waive an incomplete render");
    (void)SubmittedCancel.MarkRenderComplete(1, 0, 1);
    Check(State,
        SubmittedCancel.MarkRetiringGenerationRenderUsesComplete(1) ==
            EVulkanLabPresentationPolicyResult::Accepted,
        "submitted cancellation drains only after render completion");
}

void TestFenceRetirement(FTestState& State)
{
    FVulkanLabPresentationPolicy Policy(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)Policy.AdmitInitialGeneration(MakeGeneration(1));
    (void)Policy.RecordAcquisition(1, 0, 1);
    (void)Policy.RecordPresentationQueued(1, 0, 1);
    (void)Policy.MarkRenderComplete(1, 0, 1);
    Check(State,
        Policy.MarkPresentationFenceComplete(1, 0, 1) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.GetOutstandingRecordCount() == 0 &&
            Policy.IsImageReusable(1, 0),
        "presentation fence plus render completion releases an image");

    FVulkanLabPresentationPolicy Fallback(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)Fallback.AdmitInitialGeneration(MakeGeneration(1));
    (void)Fallback.RecordAcquisition(1, 0, 1);
    (void)Fallback.RecordPresentationQueued(1, 0, 1);
    Check(State,
        Fallback.MarkPresentationFenceComplete(1, 0, 1) ==
            EVulkanLabPresentationPolicyResult::StateConflict,
        "fallback cannot claim a presentation-fence proof");
}

void TestGenerationBoundsAndReplacement(FTestState& State)
{
    FVulkanLabPresentationPolicy Policy(
        ERHIPresentationRetirementMode::AcquireHistory);
    const auto Small = MakeGeneration(1, 8, 64, 64);
    Check(State,
        Policy.AdmitInitialGeneration(Small) ==
            EVulkanLabPresentationPolicyResult::Accepted,
        "eight-image generation is admitted");

    const auto TooLarge = MakeGeneration(
        2,
        8,
        4096,
        1920,
        ERHIFormat::R32G32B32A32_Float);
    Check(State,
        Policy.QueueReplacement(TooLarge) ==
            EVulkanLabPresentationPolicyResult::BudgetExceeded,
        "checked color storage rejects an over-512-MiB generation");

    const auto Replacement = MakeGeneration(2, 8, 1024, 1024);
    Check(State,
        Policy.QueueReplacement(Replacement) ==
            EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.QueueReplacement(MakeGeneration(3, 8, 512, 512)) ==
                EVulkanLabPresentationPolicyResult::Coalesced,
        "latest valid replacement coalesces without allocating a third generation");
    Check(State,
        Policy.BeginPendingReplacement() ==
            EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.CompletePendingReplacement(true) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.GetActiveGeneration().Desc.Generation == 3 &&
            Policy.GetRetiringGeneration().Desc.Generation == 1,
        "replacement retains one predecessor and promotes only the latest request");

    Check(State,
        Policy.QueueReplacement(MakeGeneration(4, 2, 128, 128)) ==
            EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.BeginPendingReplacement() ==
                EVulkanLabPresentationPolicyResult::GenerationLimit,
        "third-generation admission is blocked while predecessor retires");

    // The active and retiring generations may each own all eight image
    // records.  A same-image reacquisition stays in the existing record until
    // its acquire synchronization completes, preserving the sixteen-record
    // cap while still allowing retirement to make progress.
    FVulkanLabPresentationPolicy Full(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)Full.AdmitInitialGeneration(MakeGeneration(1, 8, 64, 64));
    for (uint32 Image = 0; Image < 8; ++Image)
    {
        const uint64 Token = 100 + Image;
        (void)Full.RecordAcquisition(1, Image, Token);
        (void)Full.RecordPresentationQueued(1, Image, Token);
        (void)Full.MarkRenderComplete(1, Image, Token);
    }
    (void)Full.QueueReplacement(MakeGeneration(2, 8, 64, 64));
    (void)Full.BeginPendingReplacement();
    (void)Full.CompletePendingReplacement(true);
    for (uint32 Image = 0; Image < 8; ++Image)
    {
        const uint64 Token = 200 + Image;
        (void)Full.RecordAcquisition(2, Image, Token);
        (void)Full.RecordPresentationQueued(2, Image, Token);
        (void)Full.MarkRenderComplete(2, Image, Token);
    }
    Check(State,
        Full.GetOutstandingRecordCount() ==
                FVulkanLabPresentationPolicy::MaxPresentationRecords &&
            Full.RecordAcquisition(2, 0, 300) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Full.GetOutstandingRecordCount() ==
                FVulkanLabPresentationPolicy::MaxPresentationRecords &&
            Full.MarkAcquireSynchronizationComplete(2, 0, 300) ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "full active plus retiring record cap admits bounded reacquisition");

    FVulkanLabPresentationPolicy OldOwners(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)OldOwners.AdmitInitialGeneration(MakeGeneration(1, 1, 128, 128));
    (void)OldOwners.RecordAcquisition(1, 0, 10);
    (void)OldOwners.RecordPresentationQueued(1, 0, 10);
    (void)OldOwners.MarkRenderComplete(1, 0, 10);
    (void)OldOwners.QueueReplacement(MakeGeneration(2, 1, 128, 128));
    (void)OldOwners.BeginPendingReplacement();
    (void)OldOwners.CompletePendingReplacement(true);
    (void)OldOwners.RecordAcquisition(2, 0, 20);
    (void)OldOwners.RecordPresentationQueued(2, 0, 20);
    (void)OldOwners.MarkRenderComplete(2, 0, 20);
    (void)OldOwners.RecordAcquisition(2, 0, 21);
    (void)OldOwners.MarkAcquireSynchronizationComplete(2, 0, 21);
    Check(State,
        OldOwners.MarkRetiringGenerationRenderUsesComplete(1) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            OldOwners.CanRetirePredecessor() &&
            OldOwners.RetirePredecessor() ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "replacement proof permits old generation teardown after render drain");

    FVulkanLabPresentationPolicy Failure(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)Failure.AdmitInitialGeneration(MakeGeneration(1, 2, 128, 128));
    (void)Failure.QueueReplacement(MakeGeneration(2, 2, 128, 128));
    (void)Failure.BeginPendingReplacement();
    Check(State,
        Failure.CompletePendingReplacement(false) ==
                EVulkanLabPresentationPolicyResult::CreationFailed &&
            !Failure.GetActiveGeneration().bValid &&
            Failure.GetRetiringGeneration().bCreationFailed &&
            Failure.WasReplacementCreationFailed() &&
            Failure.RecordAcquisition(1, 0, 1) ==
                EVulkanLabPresentationPolicyResult::Invalid,
        "failed oldSwapchain creation invalidates rollback to the old handle");
}

void TestActualImageCountCompletion(FTestState& State)
{
    FVulkanLabPresentationPolicy ActualCount(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)ActualCount.AdmitInitialGeneration(MakeGeneration(1, 1, 64, 64));
    (void)ActualCount.QueueReplacement(MakeGeneration(2, 2, 64, 64));
    (void)ActualCount.BeginPendingReplacement();
    Check(State,
        ActualCount.CompletePendingReplacement(static_cast<uint32>(4)) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            ActualCount.GetActiveGeneration().Desc.Generation == 2 &&
            ActualCount.GetActiveGeneration().Desc.ImageCount == 4 &&
            ActualCount.GetActiveGeneration().EstimatedColorBytes ==
                static_cast<uint64>(4) * 64 * 64 * 4 &&
            ActualCount.GetRetiringGeneration().Desc.Generation == 1,
        "replacement publication uses the actual image count returned by Vulkan");

    FVulkanLabPresentationPolicy TooManyImages(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)TooManyImages.AdmitInitialGeneration(MakeGeneration(1, 1, 64, 64));
    (void)TooManyImages.QueueReplacement(MakeGeneration(2, 2, 64, 64));
    (void)TooManyImages.BeginPendingReplacement();
    Check(State,
        TooManyImages.CompletePendingReplacement(static_cast<uint32>(9)) ==
                EVulkanLabPresentationPolicyResult::Invalid &&
            !TooManyImages.GetActiveGeneration().bValid &&
            TooManyImages.GetRetiringGeneration().bRetiring &&
            TooManyImages.GetRetiringGeneration().bCreationFailed &&
            TooManyImages.WasReplacementCreationFailed() &&
            TooManyImages.RecordAcquisition(1, 0, 1) ==
                EVulkanLabPresentationPolicyResult::Invalid,
        "an actual image count above the bound fails without resuming the predecessor");

    // The requested count fits the aggregate limit, but the actual count
    // returned by vkGetSwapchainImagesKHR pushes the two generations over
    // the checked 512 MiB cap.  This exercises the completion-time check
    // rather than QueueReplacement's minimum-count estimate.
    const auto LargeOld = MakeGeneration(
        1,
        1,
        4096,
        1920,
        ERHIFormat::R32G32B32A32_Float);
    const auto LargeRequested = MakeGeneration(
        2,
        3,
        4096,
        1920,
        ERHIFormat::R32G32B32A32_Float);
    FVulkanLabPresentationPolicy ActualBudget(
        ERHIPresentationRetirementMode::AcquireHistory);
    Check(State,
        ActualBudget.AdmitInitialGeneration(LargeOld) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            ActualBudget.QueueReplacement(LargeRequested) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            ActualBudget.BeginPendingReplacement() ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "a replacement minimum count can fit before actual image enumeration");
    Check(State,
        ActualBudget.CompletePendingReplacement(static_cast<uint32>(4)) ==
                EVulkanLabPresentationPolicyResult::BudgetExceeded &&
            !ActualBudget.GetActiveGeneration().bValid &&
            ActualBudget.GetRetiringGeneration().Desc.Generation == 1 &&
            ActualBudget.GetRetiringGeneration().bCreationFailed &&
            ActualBudget.WasReplacementCreationFailed() &&
            ActualBudget.RecordAcquisition(1, 0, 1) ==
                EVulkanLabPresentationPolicyResult::Invalid,
        "an actual-count aggregate budget failure retires the predecessor without rollback");
}

void TestTerminalNonPresentedOwners(FTestState& State)
{
    FVulkanLabPresentationPolicy BeforeTerminal(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)BeforeTerminal.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)BeforeTerminal.RecordAcquisition(1, 0, 401);
    (void)BeforeTerminal.CancelAcquisition(1, 0, 401);
    Check(State,
        BeforeTerminal.ResolveTerminalNonPresentedOwners() ==
            EVulkanLabPresentationPolicyResult::StateConflict,
        "non-presented terminal resolution is unavailable before terminal cleanup");

    FVulkanLabPresentationPolicy CanceledUnsubmitted(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)CanceledUnsubmitted.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)CanceledUnsubmitted.RecordAcquisition(1, 0, 402);
    (void)CanceledUnsubmitted.CancelAcquisition(1, 0, 402);
    Check(State,
        !CanceledUnsubmitted.IsImageReusable(1, 0) &&
            CanceledUnsubmitted.RecordAcquisition(1, 0, 403) ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending,
        "canceled unsubmitted ownership remains retained during normal operation");
    CanceledUnsubmitted.BeginTerminalCleanup();
    Check(State,
        CanceledUnsubmitted.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending &&
            CanceledUnsubmitted.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledUnsubmitted.GetOutstandingRecordCount() == 0 &&
            CanceledUnsubmitted.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledUnsubmitted.GetShutdownAssurance() ==
                ERHIShutdownAssurance::Proven,
        "caller-proven terminal cleanup releases a canceled unsubmitted owner");

    FVulkanLabPresentationPolicy CanceledSubmitted(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)CanceledSubmitted.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)CanceledSubmitted.RecordAcquisition(1, 0, 404);
    (void)CanceledSubmitted.MarkRenderSubmitted(1, 0, 404);
    (void)CanceledSubmitted.CancelAcquisition(1, 0, 404);
    Check(State,
        CanceledSubmitted.MarkRenderComplete(1, 0, 404) ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "a canceled submitted owner can drain its render use");
    CanceledSubmitted.BeginTerminalCleanup();
    Check(State,
        CanceledSubmitted.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledSubmitted.GetOutstandingRecordCount() == 0 &&
            CanceledSubmitted.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            CanceledSubmitted.GetShutdownAssurance() ==
                ERHIShutdownAssurance::Proven,
        "terminal cleanup releases a canceled owner after submitted render completion");

    FVulkanLabPresentationPolicy LiveSubmitted(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)LiveSubmitted.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)LiveSubmitted.RecordAcquisition(1, 0, 405);
    (void)LiveSubmitted.MarkRenderSubmitted(1, 0, 405);
    LiveSubmitted.BeginTerminalCleanup();
    Check(State,
        LiveSubmitted.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending &&
            LiveSubmitted.GetOutstandingRecordCount() == 1 &&
            LiveSubmitted.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending,
        "terminal resolution retains a submitted render until completion");
    (void)LiveSubmitted.MarkRenderComplete(1, 0, 405);
    Check(State,
        LiveSubmitted.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            LiveSubmitted.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::Accepted,
        "terminal resolution proceeds after the submitted render drains");

    FVulkanLabPresentationPolicy QueuedPresentation(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)QueuedPresentation.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)QueuedPresentation.RecordAcquisition(1, 0, 406);
    (void)QueuedPresentation.RecordPresentationQueued(1, 0, 406);
    (void)QueuedPresentation.MarkRenderComplete(1, 0, 406);
    QueuedPresentation.BeginTerminalCleanup();
    Check(State,
        QueuedPresentation.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending &&
            QueuedPresentation.GetOutstandingRecordCount() == 1 &&
            QueuedPresentation.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending,
        "terminal resolution retains a queued presentation without its completion proof");

    FVulkanLabPresentationPolicy UnresolvedReacquisition(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)UnresolvedReacquisition.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)UnresolvedReacquisition.RecordAcquisition(1, 0, 407);
    (void)UnresolvedReacquisition.RecordPresentationQueued(1, 0, 407);
    (void)UnresolvedReacquisition.MarkRenderComplete(1, 0, 407);
    (void)UnresolvedReacquisition.RecordAcquisition(1, 0, 408);
    (void)UnresolvedReacquisition.CancelAcquisition(1, 0, 408);
    UnresolvedReacquisition.BeginTerminalCleanup();
    Check(State,
        UnresolvedReacquisition.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending &&
            UnresolvedReacquisition.GetOutstandingRecordCount() == 1,
        "terminal resolution retains a canceled reacquisition before predecessor proof");
    Check(State,
        UnresolvedReacquisition.MarkAcquireSynchronizationComplete(1, 0, 408) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            UnresolvedReacquisition.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            UnresolvedReacquisition.GetOutstandingRecordCount() == 0 &&
            UnresolvedReacquisition.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            UnresolvedReacquisition.GetShutdownAssurance() ==
                ERHIShutdownAssurance::IdleAssumed,
        "acquire synchronization proves only the predecessor before terminal owner release");

    FVulkanLabPresentationPolicy Failed(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)Failed.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)Failed.RecordAcquisition(1, 0, 409);
    (void)Failed.QueueReplacement(MakeGeneration(2, 1));
    (void)Failed.BeginPendingReplacement();
    (void)Failed.CompletePendingReplacement(false);
    Failed.BeginTerminalCleanup();
    Check(State,
        Failed.WasReplacementCreationFailed() &&
            Failed.GetRetiringGeneration().bCreationFailed &&
            Failed.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Failed.GetOutstandingRecordCount() == 0 &&
            Failed.WasReplacementCreationFailed() &&
            Failed.GetRetiringGeneration().bCreationFailed,
        "failed replacement state remains diagnosed while safe terminal owners resolve");

    FVulkanLabPresentationPolicy Forced(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)Forced.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)Forced.RecordAcquisition(1, 0, 410);
    (void)Forced.CancelAcquisition(1, 0, 410);
    Forced.BeginTerminalCleanup();
    Forced.MarkForcedTermination();
    Check(State,
        Forced.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Forced.GetOutstandingRecordCount() == 1,
        "forced termination blocks terminal owner relabeling");

    FVulkanLabPresentationPolicy DeviceLost(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)DeviceLost.AdmitInitialGeneration(MakeGeneration(1, 1));
    (void)DeviceLost.RecordAcquisition(1, 0, 411);
    (void)DeviceLost.CancelAcquisition(1, 0, 411);
    DeviceLost.BeginTerminalCleanup();
    DeviceLost.MarkDeviceLost();
    Check(State,
        DeviceLost.ResolveTerminalNonPresentedOwners() ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            DeviceLost.GetOutstandingRecordCount() == 1,
        "device loss blocks terminal owner relabeling");
}

void TestPredecessorProofAndTerminalAssurance(FTestState& State)
{
    FVulkanLabPresentationPolicy Policy(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)Policy.AdmitInitialGeneration(MakeGeneration(1, 2, 128, 128));
    (void)Policy.QueueReplacement(MakeGeneration(2, 2, 128, 128));
    (void)Policy.BeginPendingReplacement();
    (void)Policy.CompletePendingReplacement(true);

    (void)Policy.RecordAcquisition(2, 0, 20);
    (void)Policy.RecordPresentationQueued(2, 0, 20);
    (void)Policy.MarkRenderComplete(2, 0, 20);
    Check(State,
        Policy.MarkRetiringGenerationRenderUsesComplete(1) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            !Policy.CanRetirePredecessor(),
        "replacement predecessor cannot retire after submit alone");
    (void)Policy.RecordAcquisition(2, 0, 21);
    (void)Policy.MarkAcquireSynchronizationComplete(2, 0, 21);
    Check(State,
        Policy.CanRetirePredecessor() &&
            Policy.RetirePredecessor() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            !Policy.GetRetiringGeneration().bValid,
        "predecessor retirement requires first replacement reacquisition proof");

    Policy.BeginTerminalCleanup();
    Check(State,
        Policy.ResolveTerminalOwnersForCompatibility() ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Policy.GetShutdownAssurance() == ERHIShutdownAssurance::Unknown,
        "terminal owners cannot be cleared before successful idle");
    Check(State,
        Policy.RecordAcquisition(2, 0, 22) ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Policy.QueueReplacement(MakeGeneration(3, 2, 128, 128)) ==
                EVulkanLabPresentationPolicyResult::StateConflict,
        "terminal cleanup rejects new acquisitions and replacements");
    Check(State,
        Policy.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.GetShutdownAssurance() == ERHIShutdownAssurance::IdleAssumed,
        "fallback terminal idle is explicitly IdleAssumed");
    Check(State,
        Policy.ResolveTerminalOwnersForCompatibility() ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Policy.GetOutstandingRecordCount() == 0,
        "successful fallback idle permits bounded compatibility cleanup");
    Policy.MarkForcedTermination();
    Check(State,
        Policy.GetShutdownAssurance() == ERHIShutdownAssurance::Forced,
        "watchdog termination remains a failed assurance");
    Check(State,
        Policy.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Policy.ResolveTerminalOwnersForCompatibility() ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Policy.GetShutdownAssurance() == ERHIShutdownAssurance::Forced,
        "forced termination stays failed through later cleanup attempts");
    Policy.MarkDeviceLost();
    Check(State,
        Policy.GetShutdownAssurance() == ERHIShutdownAssurance::DeviceLost,
        "device loss remains distinct from compatibility cleanup");

    FVulkanLabPresentationPolicy Lost(
        ERHIPresentationRetirementMode::AcquireHistory);
    (void)Lost.AdmitInitialGeneration(MakeGeneration(1, 1, 128, 128));
    Lost.BeginTerminalCleanup();
    Lost.MarkDeviceLost();
    Check(State,
        Lost.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Lost.ResolveTerminalOwnersForCompatibility() ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Lost.GetShutdownAssurance() == ERHIShutdownAssurance::DeviceLost,
        "device loss stays failed through later cleanup attempts");

    FVulkanLabPresentationPolicy Proven(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)Proven.AdmitInitialGeneration(MakeGeneration(1, 1, 128, 128));
    (void)Proven.RecordAcquisition(1, 0, 1);
    (void)Proven.RecordPresentationQueued(1, 0, 1);
    (void)Proven.MarkRenderComplete(1, 0, 1);
    (void)Proven.MarkPresentationFenceComplete(1, 0, 1);
    Proven.BeginTerminalCleanup();
    Check(State,
        Proven.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::Accepted &&
            Proven.GetShutdownAssurance() == ERHIShutdownAssurance::Proven,
        "fully observed presentation-fence cleanup is Proven");

    FVulkanLabPresentationPolicy Unproven(
        ERHIPresentationRetirementMode::PresentationFence);
    (void)Unproven.AdmitInitialGeneration(MakeGeneration(1, 1, 128, 128));
    (void)Unproven.RecordAcquisition(1, 0, 1);
    (void)Unproven.RecordPresentationQueued(1, 0, 1);
    (void)Unproven.MarkRenderComplete(1, 0, 1);
    Unproven.BeginTerminalCleanup();
    Check(State,
        Unproven.ResolveTerminalOwnersForCompatibility() ==
                EVulkanLabPresentationPolicyResult::StateConflict &&
            Unproven.CompleteTerminalIdle(true) ==
                EVulkanLabPresentationPolicyResult::ImageOwnershipPending,
        "preferred terminal cleanup cannot relabel unproven owners");
}

} // namespace

int RunVulkanLabPresentationPolicyTests()
{
    FTestState State;
    TestCapabilitySelection(State);
    TestImageIndexedRetirement(State);
    TestFenceRetirement(State);
    TestGenerationBoundsAndReplacement(State);
    TestActualImageCountCompletion(State);
    TestTerminalNonPresentedOwners(State);
    TestPredecessorProofAndTerminalAssurance(State);
    return State.Failed;
}
