#pragma once

#include "Core/CoreMinimal.h"
#include "FMetalDiagnostics.h"
#include "MetalRHI/FMetalBackendInspection.h"

#include <atomic>
#include <mutex>

namespace Stoner::Backend::Metal::Private
{

enum class EMetalOwnershipCategory : Core::uint8
{
    Resource,
    Pipeline,
    Command,
    Synchronization,
    Presentation,
    Count
};

class FMetalDeviceOwnerState
{
public:
    explicit FMetalDeviceOwnerState(Core::uint64 OwnerIdentity) noexcept;

    [[nodiscard]] Core::uint64 GetOwnerIdentity() const noexcept;
    [[nodiscard]] Core::uint64 GetGeneration() const noexcept;
    [[nodiscard]] bool IsCompatible(
        Core::uint64 OwnerIdentity,
        Core::uint64 Generation) const noexcept;
    [[nodiscard]] bool TryRegisterObject(
        EMetalOwnershipCategory Category) noexcept;
    void ReleaseObject(EMetalOwnershipCategory Category) noexcept;
    [[nodiscard]] bool TryBeginSubmission() noexcept;
    void EndSubmission() noexcept;
    void ReleaseDeviceOwnership() noexcept;
    void StopAdmission() noexcept;
    void AdvanceGeneration() noexcept;
    void RecordTerminalFailure(const Core::FString& StableReason) noexcept;
    void RecordDiagnostic(
        const Core::FString& Operation,
        const Core::FString& Context,
        RHI::ERHIResult Result,
        const Core::FString& StableReason,
        Core::uint64 ObjectIdentity = 0,
        Core::uint64 FrameIdentity = 0,
        const Core::FString& CapabilityReason = {},
        const Core::FString& RecoveryState = {}) noexcept;
    [[nodiscard]] FMetalBackendDiagnostics SnapshotDiagnostics() const;
    [[nodiscard]] bool IsShutdownReady() const noexcept;
    [[nodiscard]] FMetalBackendInspection Inspect() const noexcept;

private:
    const Core::uint64 OwnerIdentity_;
    std::atomic<Core::uint64> Generation_{1};
    std::atomic<Core::uint64> DeviceOwnershipCount_{1};
    std::atomic<Core::uint64> LiveObjectCount_{0};
    std::atomic<Core::uint64> ResourceOwnershipCount_{0};
    std::atomic<Core::uint64> PipelineOwnershipCount_{0};
    std::atomic<Core::uint64> CommandOwnershipCount_{0};
    std::atomic<Core::uint64> SynchronizationOwnershipCount_{0};
    std::atomic<Core::uint64> PresentationOwnershipCount_{0};
    std::atomic<Core::uint64> SubmissionOwnershipCount_{0};
    std::atomic<Core::uint64> InFlightSubmissionCount_{0};
    std::atomic<bool> bAcceptingWork_{true};
    mutable std::mutex FailureMutex_;
    Core::FString TerminalFailureReason_;
    FMetalDiagnostics Diagnostics_;
};

} // namespace Stoner::Backend::Metal::Private
