#include "FMetalDeviceOwnerState.h"

namespace Stoner::Backend::Metal::Private
{

FMetalDeviceOwnerState::FMetalDeviceOwnerState(
    Core::uint64 OwnerIdentity) noexcept
    : OwnerIdentity_(OwnerIdentity)
{
}

Core::uint64 FMetalDeviceOwnerState::GetOwnerIdentity() const noexcept
{
    return OwnerIdentity_;
}

Core::uint64 FMetalDeviceOwnerState::GetGeneration() const noexcept
{
    return Generation_.load(std::memory_order_acquire);
}

bool FMetalDeviceOwnerState::IsCompatible(
    Core::uint64 OwnerIdentity,
    Core::uint64 Generation) const noexcept
{
    return bAcceptingWork_.load(std::memory_order_acquire) &&
        OwnerIdentity == OwnerIdentity_ && Generation == GetGeneration();
}

namespace
{
void Decrement(std::atomic<Core::uint64>& Counter) noexcept
{
    Core::uint64 Count = Counter.load(std::memory_order_acquire);
    while (Count > 0 && !Counter.compare_exchange_weak(
        Count, Count - 1, std::memory_order_acq_rel))
    {
    }
}

} // namespace

bool FMetalDeviceOwnerState::TryRegisterObject(
    EMetalOwnershipCategory Category) noexcept
{
    if (!bAcceptingWork_.load(std::memory_order_acquire)) return false;
    LiveObjectCount_.fetch_add(1, std::memory_order_acq_rel);
    auto* Counter = &ResourceOwnershipCount_;
    switch (Category)
    {
    case EMetalOwnershipCategory::Resource: break;
    case EMetalOwnershipCategory::Pipeline:
        Counter = &PipelineOwnershipCount_; break;
    case EMetalOwnershipCategory::Command:
        Counter = &CommandOwnershipCount_; break;
    case EMetalOwnershipCategory::Synchronization:
        Counter = &SynchronizationOwnershipCount_; break;
    case EMetalOwnershipCategory::Presentation:
        Counter = &PresentationOwnershipCount_; break;
    case EMetalOwnershipCategory::Count: break;
    }
    Counter->fetch_add(1, std::memory_order_acq_rel);
    if (bAcceptingWork_.load(std::memory_order_acquire)) return true;
    Decrement(*Counter);
    Decrement(LiveObjectCount_);
    return false;
}

void FMetalDeviceOwnerState::ReleaseObject(
    EMetalOwnershipCategory Category) noexcept
{
    auto* Counter = &ResourceOwnershipCount_;
    switch (Category)
    {
    case EMetalOwnershipCategory::Resource: break;
    case EMetalOwnershipCategory::Pipeline:
        Counter = &PipelineOwnershipCount_; break;
    case EMetalOwnershipCategory::Command:
        Counter = &CommandOwnershipCount_; break;
    case EMetalOwnershipCategory::Synchronization:
        Counter = &SynchronizationOwnershipCount_; break;
    case EMetalOwnershipCategory::Presentation:
        Counter = &PresentationOwnershipCount_; break;
    case EMetalOwnershipCategory::Count: break;
    }
    Decrement(*Counter);
    Decrement(LiveObjectCount_);
}

bool FMetalDeviceOwnerState::TryBeginSubmission() noexcept
{
    if (!bAcceptingWork_.load(std::memory_order_acquire)) return false;
    InFlightSubmissionCount_.fetch_add(1, std::memory_order_acq_rel);
    SubmissionOwnershipCount_.fetch_add(1, std::memory_order_acq_rel);
    if (bAcceptingWork_.load(std::memory_order_acquire)) return true;
    Decrement(SubmissionOwnershipCount_);
    Decrement(InFlightSubmissionCount_);
    return false;
}

void FMetalDeviceOwnerState::EndSubmission() noexcept
{
    Decrement(SubmissionOwnershipCount_);
    Decrement(InFlightSubmissionCount_);
}

void FMetalDeviceOwnerState::ReleaseDeviceOwnership() noexcept
{
    DeviceOwnershipCount_.store(0, std::memory_order_release);
}

void FMetalDeviceOwnerState::StopAdmission() noexcept
{
    bAcceptingWork_.store(false, std::memory_order_release);
}

void FMetalDeviceOwnerState::AdvanceGeneration() noexcept
{
    Generation_.fetch_add(1, std::memory_order_acq_rel);
}

void FMetalDeviceOwnerState::RecordTerminalFailure(
    const Core::FString& StableReason) noexcept
{
    if (StableReason.IsEmpty()) return;
    std::lock_guard Lock(FailureMutex_);
    if (TerminalFailureReason_.IsEmpty())
        TerminalFailureReason_ = StableReason;
}

void FMetalDeviceOwnerState::RecordDiagnostic(
    const Core::FString& Operation,
    const Core::FString& Context,
    RHI::ERHIResult Result,
    const Core::FString& StableReason,
    Core::uint64 ObjectIdentity,
    Core::uint64 FrameIdentity,
    const Core::FString& CapabilityReason,
    const Core::FString& RecoveryState) noexcept
{
    try
    {
        FMetalDiagnosticRecord Record;
        Record.Operation = Operation;
        Record.Context = Context;
        Record.Result = Result;
        Record.StableReason = StableReason;
        Record.ObjectIdentity = ObjectIdentity == 0
            ? OwnerIdentity_ : ObjectIdentity;
        Record.FrameIdentity = FrameIdentity;
        Record.CapabilityReason = CapabilityReason;
        Record.RecoveryState = RecoveryState;
        Diagnostics_.Add(std::move(Record));
    }
    catch (...)
    {
    }
}

FMetalBackendDiagnostics FMetalDeviceOwnerState::SnapshotDiagnostics() const
{
    return Diagnostics_.Snapshot();
}

bool FMetalDeviceOwnerState::IsShutdownReady() const noexcept
{
    return !bAcceptingWork_.load(std::memory_order_acquire) &&
        InFlightSubmissionCount_.load(std::memory_order_acquire) == 0;
}

FMetalBackendInspection FMetalDeviceOwnerState::Inspect() const noexcept
{
    FMetalBackendInspection Result;
    Result.OwnerIdentity = OwnerIdentity_;
    Result.Generation = GetGeneration();
    Result.DeviceOwnershipCount =
        DeviceOwnershipCount_.load(std::memory_order_acquire);
    Result.LiveObjectCount =
        LiveObjectCount_.load(std::memory_order_acquire);
    Result.ResourceOwnershipCount =
        ResourceOwnershipCount_.load(std::memory_order_acquire);
    Result.PipelineOwnershipCount =
        PipelineOwnershipCount_.load(std::memory_order_acquire);
    Result.CommandOwnershipCount =
        CommandOwnershipCount_.load(std::memory_order_acquire);
    Result.SynchronizationOwnershipCount =
        SynchronizationOwnershipCount_.load(std::memory_order_acquire);
    Result.SubmissionOwnershipCount =
        SubmissionOwnershipCount_.load(std::memory_order_acquire);
    Result.PresentationOwnershipCount =
        PresentationOwnershipCount_.load(std::memory_order_acquire);
    Result.InFlightSubmissionCount =
        InFlightSubmissionCount_.load(std::memory_order_acquire);
    Result.bAcceptingWork =
        bAcceptingWork_.load(std::memory_order_acquire);
    {
        std::lock_guard Lock(FailureMutex_);
        Result.TerminalFailureReason = TerminalFailureReason_;
    }
    Result.bTerminalFailure = !Result.TerminalFailureReason.IsEmpty();
    return Result;
}

} // namespace Stoner::Backend::Metal::Private
