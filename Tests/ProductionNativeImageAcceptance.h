#pragma once

#include "Core/CoreMinimal.h"
#include "ProductionImageAcceptance.h"

namespace Stoner::Demo
{
struct FDemoProductionExecutionInspection;
}

struct FProductionReferenceComparison
{
    Stoner::Core::FString ReferenceId;
    FProductionFlipResult Flip;
};

struct FProductionNativeImageAcceptanceResult
{
    bool bPassed = false;
    Stoner::Core::FString DeviceClass;
    Stoner::Core::FString BaselineId;
    Stoner::Core::FString MatchedReferenceId;
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::FString FirstFailure;
    FProductionSemanticProbeResult Semantic;
    FProductionFlipResult Flip;
    Stoner::Core::TArray<FProductionReferenceComparison> ReferenceComparisons;
};

[[nodiscard]] bool ValidateProductionAuthoritativeFrameBundle(
    const Stoner::Demo::FDemoProductionExecutionInspection& Inspection,
    Stoner::Core::FString& OutFailure);

[[nodiscard]] bool SelectProductionMatchedReference(
    const Stoner::Core::TArray<FProductionReferenceComparison>& Comparisons,
    Stoner::Core::FString& OutReferenceId,
    FProductionFlipResult& OutFlip);

[[nodiscard]] bool BuildProductionWorkloadRegions(
    const Stoner::Core::FString& WorkloadRevision,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::Core::TArray<FProductionRegionProbe>& OutRegions);

[[nodiscard]] bool IsProductionWorkloadNormalProbeValid(
    const Stoner::Core::FString& WorkloadRevision,
    const Stoner::Core::FVector3& WorldNormal) noexcept;

[[nodiscard]] FProductionNativeImageAcceptanceResult
RunProductionNativeImageAcceptance(
    const Stoner::Demo::FDemoProductionExecutionInspection& Inspection,
    const Stoner::Core::FString& Backend,
    const Stoner::Core::FString& TargetProfilePath,
    const Stoner::Core::FString& WorkloadRevision,
    const Stoner::Core::FString& BaselineRoot,
    const Stoner::Core::FString& DeviceClassRegistryPath,
    const Stoner::Core::FString& CaptureRoot);

void PrintProductionNativeImageEvidence(
    const Stoner::Core::FString& Backend,
    const FProductionNativeImageAcceptanceResult& Result);

void PrintProductionReadbackDiagnostics(
    const Stoner::Demo::FDemoProductionExecutionInspection& Inspection);
