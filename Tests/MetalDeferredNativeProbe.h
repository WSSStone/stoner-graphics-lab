#pragma once

#include "Core/CoreMinimal.h"

enum class EMetalDeferredProbeStatus
{
    Success,
    Unavailable,
    Failed
};

struct FMetalDeferredNativeProbeReport
{
    EMetalDeferredProbeStatus Status = EMetalDeferredProbeStatus::Failed;
    Stoner::Core::FString StableReason;
    bool bUsedSharedRenderer = false;
    bool bNativeSubmissionCompleted = false;
    bool bGBufferPassed = false;
    bool bWorldNormalPassed = false;
    bool bDepthPassed = false;
    bool bLightingPassed = false;
    bool bFinalOutputPassed = false;
    Stoner::Core::TArray<Stoner::Core::FString> ShaderEvidenceDigests;
    Stoner::Core::FString FinalOutputDigest;
    Stoner::Core::FVector4 BaseColorAO;
    Stoner::Core::FVector4 NormalRoughness;
    Stoner::Core::FVector4 EmissiveMetallic;
    Stoner::Core::FVector4 Depth;
    Stoner::Core::FVector4 Lighting;
    Stoner::Core::FVector4 FinalOutput;
};

[[nodiscard]] FMetalDeferredNativeProbeReport
RunMetalDeferredNativeProbe();
