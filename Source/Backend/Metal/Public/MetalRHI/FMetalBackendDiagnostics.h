#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::Backend::Metal
{

struct FMetalDiagnosticRecord
{
    FMetalDiagnosticRecord() = default;
    FMetalDiagnosticRecord(
        Core::FString InOperation,
        Core::FString InContext,
        RHI::ERHIResult InResult,
        Core::FString InStableReason)
        : Operation(std::move(InOperation)),
          Context(std::move(InContext)),
          Result(InResult),
          StableReason(std::move(InStableReason))
    {
    }

    Core::FString Operation;
    Core::FString Context;
    RHI::ERHIResult Result = RHI::ERHIResult::Failed;
    Core::FString StableReason;
    Core::FString Backend = Core::FString("Metal");
    Core::uint64 ObjectIdentity = 0;
    Core::uint64 FrameIdentity = 0;
    Core::FString CapabilityReason;
    Core::FString RecoveryState;
};

struct FMetalAdapterSummary
{
    Core::uint64 RegistryId = 0;
    Core::FString Name;
    bool bLowPower = false;
    bool bRemovable = false;
    bool bHeadless = false;
};

struct FMetalBackendDiagnostics
{
    Core::TArray<FMetalAdapterSummary> Candidates;
    FMetalAdapterSummary SelectedAdapter;
    Core::TArray<FMetalDiagnosticRecord> Records;
    bool bTruncated = false;
};

} // namespace Stoner::Backend::Metal
