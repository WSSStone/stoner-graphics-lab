#include "Renderer/FRendererComparisonReport.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Stoner::Renderer
{

namespace
{

double Percentile(Stoner::Core::TArray<double> Values, double Fraction)
{
    std::sort(Values.begin(), Values.end());
    const std::size_t Index = static_cast<std::size_t>(
        std::ceil(Fraction * static_cast<double>(Values.size()))) - 1;
    return Values[std::min(Index, Values.size() - 1)];
}

bool IsRequiredTier(Stoner::Core::uint32 Tier) noexcept
{
    return Tier == 0 || Tier == 16 || Tier == 64 || Tier == 256;
}

} // namespace

Stoner::Core::FString FRendererWorkloadFingerprint::GetIdentity() const
{
    std::ostringstream Stream;
    Stream << Scene.CStr() << '|' << View.CStr() << '|' << Materials.CStr() << '|'
        << Lights.CStr() << "|draws=" << OpaqueDrawCount << "|lights=" << AcceptedLightCount;
    return Stoner::Core::FString(Stream.str());
}

bool FRendererWorkloadFingerprint::IsEquivalent(
    const FRendererWorkloadFingerprint& Other) const
{
    return GetIdentity() == Other.GetIdentity();
}

bool FinalizeRendererComparisonTier(FRendererComparisonTier& Tier,
    FDeferredDiagnosticLog* Diagnostics)
{
    Tier.bValid = false;
    if (!IsRequiredTier(Tier.LocalLightCount) || Tier.WarmupFrames == 0 ||
        Tier.Samples.size() < 100 ||
        !Tier.ForwardFingerprint.IsEquivalent(Tier.DeferredFingerprint) ||
        Tier.ForwardFingerprint.AcceptedLightCount != Tier.LocalLightCount ||
        Tier.DeferredSurfaceDrawCount != Tier.DeferredFingerprint.OpaqueDrawCount)
    {
        if (Diagnostics)
        {
            Diagnostics->Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::Composition,
                EDeferredResult::ComparisonInvalid, "DEF-COMPARE-TIER", "RendererComparison",
                "tier requires equivalent fingerprints warmup and at least 100 measured samples");
        }
        return false;
    }
    Stoner::Core::TArray<double> Forward;
    Stoner::Core::TArray<double> Deferred;
    for (const FRendererTimingSample& Sample : Tier.Samples)
    {
        if (!std::isfinite(Sample.ForwardMilliseconds) ||
            !std::isfinite(Sample.DeferredMilliseconds) ||
            Sample.ForwardMilliseconds < 0.0 || Sample.DeferredMilliseconds < 0.0)
        {
            if (Diagnostics)
            {
                Diagnostics->Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::Composition,
                    EDeferredResult::ComparisonInvalid, "DEF-COMPARE-SAMPLE",
                    "RendererComparison", "timing samples must be finite and non-negative");
            }
            return false;
        }
        Forward.push_back(Sample.ForwardMilliseconds);
        Deferred.push_back(Sample.DeferredMilliseconds);
    }
    Tier.ForwardMedianMilliseconds = Percentile(Forward, 0.5);
    Tier.ForwardP95Milliseconds = Percentile(Forward, 0.95);
    Tier.DeferredMedianMilliseconds = Percentile(Deferred, 0.5);
    Tier.DeferredP95Milliseconds = Percentile(Deferred, 0.95);
    Tier.bValid = true;
    return true;
}

FRendererComparisonReport BuildRendererComparisonReport(
    Stoner::Core::TArray<FRendererComparisonTier> Tiers)
{
    FRendererComparisonReport Report;
    std::sort(Tiers.begin(), Tiers.end(), [](const FRendererComparisonTier& Left,
        const FRendererComparisonTier& Right) {
        return Left.LocalLightCount < Right.LocalLightCount;
    });
    const Stoner::Core::uint32 Required[] = {0, 16, 64, 256};
    if (Tiers.size() != 4)
    {
        Report.State = ERendererComparisonState::Invalid;
        return Report;
    }
    Stoner::Core::uint32 ConstantSurfaceDraws = 0;
    for (std::size_t Index = 0; Index < Tiers.size(); ++Index)
    {
        if (Tiers[Index].LocalLightCount != Required[Index] ||
            !FinalizeRendererComparisonTier(Tiers[Index], &Report.Diagnostics))
        {
            Report.State = ERendererComparisonState::Invalid;
            Report.Tiers = std::move(Tiers);
            return Report;
        }
        if (Index == 0)
        {
            ConstantSurfaceDraws = Tiers[Index].DeferredSurfaceDrawCount;
        }
        else if (Tiers[Index].DeferredSurfaceDrawCount != ConstantSurfaceDraws)
        {
            Report.Diagnostics.Add(EDeferredDiagnosticSeverity::Error,
                EDeferredPassStage::SurfaceData, EDeferredResult::ComparisonInvalid,
                "DEF-COMPARE-SURFACE-WORK", "RendererComparison",
                "deferred surface geometry work must remain constant across light tiers");
            Report.State = ERendererComparisonState::Invalid;
            Report.Tiers = std::move(Tiers);
            return Report;
        }
    }
    for (const FRendererComparisonTier& Tier : Tiers)
    {
        if (Tier.LocalLightCount > 0 &&
            Tier.DeferredMedianMilliseconds < Tier.ForwardMedianMilliseconds)
        {
            Report.Crossover = Tier.LocalLightCount == 16
                ? ERendererCrossoverClassification::DeferredAt16
                : (Tier.LocalLightCount == 64
                    ? ERendererCrossoverClassification::DeferredAt64
                    : ERendererCrossoverClassification::DeferredAt256);
            break;
        }
    }
    Report.State = ERendererComparisonState::Valid;
    Report.Tiers = std::move(Tiers);
    return Report;
}

Stoner::Core::FString FRendererComparisonReport::Dump() const
{
    std::ostringstream Stream;
    Stream << "RendererComparison state=" << (IsValid() ? "Valid" : "Invalid")
        << " crossover=" << ToString(Crossover) << '\n';
    Stream << std::fixed << std::setprecision(6);
    for (const FRendererComparisonTier& Tier : Tiers)
    {
        Stream << "Tier localLights=" << Tier.LocalLightCount << " warmup="
            << Tier.WarmupFrames << " samples=" << Tier.Samples.size()
            << " fingerprint=" << Tier.ForwardFingerprint.GetIdentity().CStr()
            << " forwardMedianMs=" << Tier.ForwardMedianMilliseconds
            << " forwardP95Ms=" << Tier.ForwardP95Milliseconds
            << " deferredMedianMs=" << Tier.DeferredMedianMilliseconds
            << " deferredP95Ms=" << Tier.DeferredP95Milliseconds
            << " forwardGeometryDraws=" << Tier.ForwardGeometryDrawCount
            << " deferredSurfaceDraws=" << Tier.DeferredSurfaceDrawCount
            << " deferredLocalLightDraws=" << Tier.DeferredLocalLightDrawCount
            << " culledLocalLights=" << Tier.CulledLocalLightCount << '\n';
    }
    Stream << Diagnostics.Dump().CStr();
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(ERendererCrossoverClassification Classification) noexcept
{
    switch (Classification)
    {
    case ERendererCrossoverClassification::NoneObserved: return "NoneObserved";
    case ERendererCrossoverClassification::DeferredAt16: return "DeferredAt16";
    case ERendererCrossoverClassification::DeferredAt64: return "DeferredAt64";
    case ERendererCrossoverClassification::DeferredAt256: return "DeferredAt256";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
