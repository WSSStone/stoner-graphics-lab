#include "RendererComparisonTests.h"

#include "Renderer/FRendererComparisonReport.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace
{

using namespace Stoner::Renderer;

void Record(FRendererComparisonTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

FRendererComparisonTier MakeTier(unsigned int LightCount)
{
    FRendererComparisonTier Tier;
    Tier.LocalLightCount = LightCount;
    Tier.WarmupFrames = 20;
    Tier.ForwardFingerprint = {"Scene019", "View019", "Materials019",
        Stoner::Core::FString(std::to_string(LightCount)), 100, LightCount};
    Tier.DeferredFingerprint = Tier.ForwardFingerprint;
    Tier.ForwardGeometryDrawCount = 100;
    Tier.DeferredSurfaceDrawCount = 100;
    Tier.DeferredLocalLightDrawCount = LightCount;
    for (int Index = 0; Index < 100; ++Index)
    {
        Tier.Samples.push_back({1.0 + LightCount * 0.01 + Index * 0.0001,
            1.5 + LightCount * 0.002 + Index * 0.0001});
    }
    return Tier;
}

} // namespace

FRendererComparisonTestResult RunRendererComparisonTests()
{
    FRendererComparisonTestResult Result;
    auto Report = BuildRendererComparisonReport({
        MakeTier(256), MakeTier(0), MakeTier(64), MakeTier(16)});
    Record(Result, Report.IsValid() && Report.Tiers.size() == 4 &&
        Report.Tiers[0].LocalLightCount == 0 && Report.Tiers[3].LocalLightCount == 256,
        "Renderer comparison normalizes all four required local-light tiers");
    Record(Result, Report.Tiers[2].Samples.size() == 100 &&
        Report.Tiers[2].ForwardP95Milliseconds >= Report.Tiers[2].ForwardMedianMilliseconds &&
        Report.Tiers[2].DeferredP95Milliseconds >= Report.Tiers[2].DeferredMedianMilliseconds,
        "Renderer comparison reports complete median and p95 timing samples");
    Record(Result, Report.Crossover == ERendererCrossoverClassification::DeferredAt64,
        "Renderer comparison classifies first observed deferred crossover without speedup gate");

    auto MissingTierReport = BuildRendererComparisonReport({MakeTier(0), MakeTier(16), MakeTier(64)});
    Record(Result, !MissingTierReport.IsValid() && MissingTierReport.Tiers.size() == 3 &&
            MissingTierReport.Diagnostics.Dump().View().find("DEF-COMPARE-TIER-COUNT") != std::string_view::npos,
        "Renderer comparison reports missing tier count diagnostics");

    auto ExtraTierReport = BuildRendererComparisonReport({
        MakeTier(0), MakeTier(16), MakeTier(64), MakeTier(256), MakeTier(32)});
    Record(Result, !ExtraTierReport.IsValid() && ExtraTierReport.Tiers.size() == 5 &&
            ExtraTierReport.Diagnostics.Dump().View().find("DEF-COMPARE-TIER-COUNT") != std::string_view::npos,
        "Renderer comparison reports extra tier count diagnostics");
    const char* ReportPath = std::getenv("STONER_RENDERER_COMPARISON_REPORT");
    if (ReportPath != nullptr && *ReportPath != '\0')
    {
        std::ofstream Stream(ReportPath, std::ios::binary | std::ios::trunc);
        Stream << Report.DumpValidationArtifact().CStr();
        Record(Result, Stream.good(),
            "Renderer comparison writes the executed four-tier validation artifact");
    }

    auto Mismatch = MakeTier(16);
    Mismatch.DeferredFingerprint.Scene = "DifferentScene";
    Record(Result, !FinalizeRendererComparisonTier(Mismatch),
        "Renderer comparison rejects mismatched normalized fingerprints");
    auto Incomplete = MakeTier(64);
    Incomplete.Samples.resize(99);
    Record(Result, !FinalizeRendererComparisonTier(Incomplete),
        "Renderer comparison rejects incomplete measured frame set");
    auto NonFinite = MakeTier(256);
    NonFinite.Samples[0].DeferredMilliseconds = std::numeric_limits<double>::infinity();
    Record(Result, !FinalizeRendererComparisonTier(NonFinite),
        "Renderer comparison rejects non-finite timing samples");
    return Result;
}
