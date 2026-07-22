#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/FRHIRuntimeSnapshot.h"
#include "FDemoConfiguration.h"

namespace Stoner::Demo
{

class FDemoDiagnostics;

struct FDemoMemorySample
{
    Stoner::Core::uint32 CompletedFrame = 0;
    Stoner::Core::uint64 ResidentBytes = 0;
    Stoner::RHI::FRHIRuntimeSnapshot RuntimeSnapshot;
};

class FDemoValidationMonitor
{
public:
    explicit FDemoValidationMonitor(const FDemoConfiguration& InConfiguration);

    bool Sample(Stoner::Core::uint32 CompletedFrame, const Stoner::RHI::FRHIRuntimeSnapshot& RuntimeSnapshot);
    void AddSyntheticSample(Stoner::Core::uint32 CompletedFrame, Stoner::Core::uint64 ResidentBytes,
        const Stoner::RHI::FRHIRuntimeSnapshot& RuntimeSnapshot);
    void SetRequestedFrames(Stoner::Core::uint32 Value) noexcept { RequestedFrames = Value; }
    void SetCompletedFrames(Stoner::Core::uint32 Value) noexcept { CompletedFrames = Value; }
    void SetRuntimeSnapshot(const Stoner::RHI::FRHIRuntimeSnapshot& Value) { FinalSnapshot = Value; }
    void SetFirstPresentMilliseconds(double Value) noexcept { FirstPresentMilliseconds = Value; }
    void AddRecoveryMilliseconds(double Value) { RecoveryMilliseconds.push_back(Value); }

    [[nodiscard]] bool Evaluate();
    [[nodiscard]] bool Passed() const noexcept { return bPassed; }
    [[nodiscard]] Stoner::Core::uint64 GetBaselineMedianBytes() const noexcept { return BaselineMedianBytes; }
    [[nodiscard]] Stoner::Core::uint64 GetFinalMedianBytes() const noexcept { return FinalMedianBytes; }
    [[nodiscard]] Stoner::Core::FString BuildReport(const FDemoDiagnostics& Diagnostics) const;
    [[nodiscard]] bool WriteReport(const FDemoDiagnostics& Diagnostics) const;

private:
    FDemoConfiguration Configuration;
    Stoner::Core::TArray<FDemoMemorySample> Samples;
    Stoner::Core::TArray<double> RecoveryMilliseconds;
    Stoner::RHI::FRHIRuntimeSnapshot FinalSnapshot;
    Stoner::RHI::FRHIRuntimeSnapshot RuntimeProofSnapshot;
    Stoner::Core::uint32 RequestedFrames = 0;
    Stoner::Core::uint32 CompletedFrames = 0;
    Stoner::Core::uint64 BaselineMedianBytes = 0;
    Stoner::Core::uint64 FinalMedianBytes = 0;
    double FirstPresentMilliseconds = 0.0;
    bool bMemoryAvailable = true;
    bool bPassed = false;
};

} // namespace Stoner::Demo
