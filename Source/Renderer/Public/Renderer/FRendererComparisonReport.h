#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FDeferredDiagnostics.h"

namespace Stoner::Renderer
{

enum class ERendererComparisonState
{
    Draft,
    Valid,
    Invalid
};

enum class ERendererCrossoverClassification
{
    NoneObserved,
    DeferredAt16,
    DeferredAt64,
    DeferredAt256
};

struct FRendererWorkloadFingerprint
{
    Stoner::Core::FString Scene;
    Stoner::Core::FString View;
    Stoner::Core::FString Materials;
    Stoner::Core::FString Lights;
    Stoner::Core::uint32 OpaqueDrawCount = 0;
    Stoner::Core::uint32 AcceptedLightCount = 0;

    [[nodiscard]] Stoner::Core::FString GetIdentity() const;
    [[nodiscard]] bool IsEquivalent(const FRendererWorkloadFingerprint& Other) const;
};

struct FRendererTimingSample
{
    double ForwardMilliseconds = 0.0;
    double DeferredMilliseconds = 0.0;
};

struct FRendererComparisonTier
{
    Stoner::Core::uint32 LocalLightCount = 0;
    Stoner::Core::uint32 WarmupFrames = 0;
    FRendererWorkloadFingerprint ForwardFingerprint;
    FRendererWorkloadFingerprint DeferredFingerprint;
    Stoner::Core::TArray<FRendererTimingSample> Samples;
    double ForwardMedianMilliseconds = 0.0;
    double ForwardP95Milliseconds = 0.0;
    double DeferredMedianMilliseconds = 0.0;
    double DeferredP95Milliseconds = 0.0;
    Stoner::Core::uint32 ForwardGeometryDrawCount = 0;
    Stoner::Core::uint32 DeferredSurfaceDrawCount = 0;
    Stoner::Core::uint32 DeferredLocalLightDrawCount = 0;
    Stoner::Core::uint32 CulledLocalLightCount = 0;
    bool bValid = false;
};

struct FRendererComparisonReport
{
    ERendererComparisonState State = ERendererComparisonState::Draft;
    ERendererCrossoverClassification Crossover = ERendererCrossoverClassification::NoneObserved;
    Stoner::Core::TArray<FRendererComparisonTier> Tiers;
    FDeferredDiagnosticLog Diagnostics;

    [[nodiscard]] bool IsValid() const noexcept { return State == ERendererComparisonState::Valid; }
    [[nodiscard]] Stoner::Core::FString Dump() const;
};

[[nodiscard]] bool FinalizeRendererComparisonTier(FRendererComparisonTier& Tier,
    FDeferredDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] FRendererComparisonReport BuildRendererComparisonReport(
    Stoner::Core::TArray<FRendererComparisonTier> Tiers);
[[nodiscard]] const char* ToString(ERendererCrossoverClassification Classification) noexcept;

} // namespace Stoner::Renderer
