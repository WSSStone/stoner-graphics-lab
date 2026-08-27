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

struct FDemoProductionLifecycleCounters
{
    Stoner::Core::uint64 AssetOwners = 0;
    Stoner::Core::uint64 RendererOwners = 0;
    Stoner::Core::uint64 RHIObjects = 0;
    Stoner::Core::uint64 NativeObjects = 0;
    Stoner::Core::uint64 PresentationObjects = 0;
    bool bStaleHandleRejected = true;

    [[nodiscard]] bool IsAtBaseline() const noexcept
    {
        return AssetOwners == 0 && RendererOwners == 0 && RHIObjects == 0 &&
            NativeObjects == 0 && PresentationObjects == 0;
    }
};

struct FDemoProductionLifecycleSample
{
    Stoner::Core::uint32 CompletedCycle = 0;
    Stoner::Core::uint64 ResidentBytes = 0;
    FDemoProductionLifecycleCounters Counters;
    Stoner::Core::FProcessMemorySnapshot ProcessMemory;
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
    [[nodiscard]] bool SampleProductionCycle(
        Stoner::Core::uint32 CompletedCycle,
        const FDemoProductionLifecycleCounters& Counters);
    void AddSyntheticProductionCycle(
        Stoner::Core::uint32 CompletedCycle,
        Stoner::Core::uint64 ResidentBytes,
        const FDemoProductionLifecycleCounters& Counters);

    [[nodiscard]] bool Evaluate();
    [[nodiscard]] bool EvaluateProductionLifecycle();
    [[nodiscard]] bool Passed() const noexcept { return bPassed; }
    [[nodiscard]] Stoner::Core::uint64 GetBaselineMedianBytes() const noexcept { return BaselineMedianBytes; }
    [[nodiscard]] Stoner::Core::uint64 GetFinalMedianBytes() const noexcept { return FinalMedianBytes; }
    [[nodiscard]] Stoner::Core::uint64 GetProductionWarmupBytes() const noexcept
    {
        return ProductionWarmupBytes;
    }
    [[nodiscard]] Stoner::Core::uint64 GetProductionTerminalBytes() const noexcept
    {
        return ProductionTerminalBytes;
    }
    [[nodiscard]] Stoner::Core::uint64 GetProductionPeakBytes() const noexcept
    {
        return ProductionPeakBytes;
    }
    [[nodiscard]] bool IsProductionRssWithinLimit() const noexcept
    {
        return bProductionRssWithinLimit;
    }
    [[nodiscard]] const Stoner::Core::TArray<FDemoProductionLifecycleSample>&
        GetProductionSamples() const noexcept
    {
        return ProductionSamples;
    }
    [[nodiscard]] Stoner::Core::FString BuildReport(const FDemoDiagnostics& Diagnostics) const;
    [[nodiscard]] bool WriteReport(const FDemoDiagnostics& Diagnostics) const;

private:
    FDemoConfiguration Configuration;
    Stoner::Core::TArray<FDemoMemorySample> Samples;
    Stoner::Core::TArray<double> RecoveryMilliseconds;
    Stoner::Core::TArray<FDemoProductionLifecycleSample> ProductionSamples;
    Stoner::RHI::FRHIRuntimeSnapshot FinalSnapshot;
    Stoner::RHI::FRHIRuntimeSnapshot RuntimeProofSnapshot;
    Stoner::Core::uint32 RequestedFrames = 0;
    Stoner::Core::uint32 CompletedFrames = 0;
    Stoner::Core::uint64 BaselineMedianBytes = 0;
    Stoner::Core::uint64 FinalMedianBytes = 0;
    Stoner::Core::uint64 ProductionWarmupBytes = 0;
    Stoner::Core::uint64 ProductionTerminalBytes = 0;
    Stoner::Core::uint64 ProductionPeakBytes = 0;
    double FirstPresentMilliseconds = 0.0;
    bool bMemoryAvailable = true;
    bool bProductionRssWithinLimit = false;
    bool bPassed = false;
};

} // namespace Stoner::Demo
