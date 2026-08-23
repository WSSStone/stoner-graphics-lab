#pragma once

#include "Core/CoreMinimal.h"
#include "ProductionImageAcceptance.h"

namespace Stoner::Demo
{
struct FDemoProductionExecutionInspection;
}

struct FProductionNativeImageAcceptanceResult
{
    bool bPassed = false;
    Stoner::Core::FString DeviceClass;
    Stoner::Core::FString BaselineId;
    Stoner::Core::FString FirstFailure;
    FProductionSemanticProbeResult Semantic;
    FProductionFlipResult Flip;
};

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
