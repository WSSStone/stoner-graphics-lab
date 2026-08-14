#include "AssetCookerBenchmark.h"

#include "AssetCookerDerivedDataTestSupport.h"
#include "AssetCookerTestSupport.h"
#include "Core/FPlatformMemory.h"
#include "FAssetCookGraph.h"
#include "FAssetCookScheduler.h"
#include "yyjson/yyjson.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <span>
#include <string>

namespace
{

using namespace Stoner;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Tests::AssetCooker;
using namespace Stoner::Tests::AssetCookerDDC;

struct FLimits
{
    double Plan = 2.0;
    double Cached = 10.0;
    double Validate = 10.0;
    double Clean = 60.0;
    double Representative = 60.0;
    Core::uint64 PeakRss = 1ULL << 30U;
};

struct FMetrics
{
    double Plan = 0.0;
    double Cached = 0.0;
    double Validate = 0.0;
    double Clean = 0.0;
    double Representative = 0.0;
    Core::uint64 PeakRss = 0;
    Core::uint64 PayloadBytes = 0;
    Core::uint32 Assets = 0;
    Core::uint64 Edges = 0;
    Asset::FAssetDigest CorpusDigest;
    Asset::FAssetDigest ArtifactDigest;
    Asset::FAssetDigest RepresentativeGeneration;
};

void Record(FAssetCookerBenchmarkResult& Result, bool Passed, const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

template <typename Operation>
double Measure(Operation&& Work)
{
    const auto Start = std::chrono::steady_clock::now();
    Work();
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now() - Start).count();
}

bool ParseId(std::string_view Text, Asset::FAssetId& Out)
{
    const auto Colon = Text.find(':');
    return Colon != std::string_view::npos && Colon != 0 &&
        Asset::FAssetId::Create(
            Core::FString(Text.substr(0, Colon)),
            Core::FString(Text.substr(Colon + 1)), {}, Out) ==
            Asset::EAssetResult::Success;
}

bool LoadCorpus(
    const std::filesystem::path& Path,
    Core::TArray<Asset::FAssetImportOutput>& Out,
    Core::uint64& OutEdges,
    Asset::FAssetDigest& OutDigest)
{
    const auto Bytes = Read(Path);
    OutDigest = Asset::FAssetDigest::FromBytes(Bytes);
    if (Bytes.empty()) return false;
    yyjson_doc* Document = yyjson_read_opts(
        const_cast<char*>(reinterpret_cast<const char*>(Bytes.data())),
        Bytes.size(), YYJSON_READ_NOFLAG, nullptr, nullptr);
    if (!Document) return false;
    yyjson_val* Root = yyjson_doc_get_root(Document);
    yyjson_val* Assets = yyjson_obj_get(Root, "assets");
    const bool HeaderValid = yyjson_is_obj(Root) && yyjson_is_arr(Assets) &&
        yyjson_get_uint(yyjson_obj_get(Root, "assetCount")) == 1000 &&
        yyjson_get_uint(yyjson_obj_get(Root, "dependencyEdgeCount")) == 5000;
    if (!HeaderValid)
    {
        yyjson_doc_free(Document);
        return false;
    }
    std::map<std::string, Asset::FAssetId> Known;
    std::size_t Index = 0, Maximum = 0;
    yyjson_val* Value = nullptr;
    bool Valid = true;
    Out.clear();
    OutEdges = 0;
    yyjson_arr_foreach(Assets, Index, Maximum, Value)
    {
        yyjson_val* IdValue = yyjson_obj_get(Value, "id");
        yyjson_val* Dependencies = yyjson_obj_get(Value, "dependencies");
        if (!yyjson_is_str(IdValue) || !yyjson_is_arr(Dependencies))
        {
            Valid = false;
            break;
        }
        const std::string IdText(yyjson_get_str(IdValue), yyjson_get_len(IdValue));
        Asset::FAssetId AssetId;
        if (!ParseId(IdText, AssetId) || Known.contains(IdText))
        {
            Valid = false;
            break;
        }
        Core::TArray<Asset::FAssetDependency> ParsedDependencies;
        std::size_t DependencyIndex = 0, DependencyMaximum = 0;
        yyjson_val* Dependency = nullptr;
        yyjson_arr_foreach(
            Dependencies, DependencyIndex, DependencyMaximum, Dependency)
        {
            if (!yyjson_is_str(Dependency))
            {
                Valid = false;
                break;
            }
            const std::string DependencyText(
                yyjson_get_str(Dependency), yyjson_get_len(Dependency));
            const auto Found = Known.find(DependencyText);
            if (Found == Known.end())
            {
                Valid = false;
                break;
            }
            ParsedDependencies.push_back(Required(Found->second));
        }
        if (!Valid) break;
        OutEdges += ParsedDependencies.size();
        Out.push_back(Output(AssetId, std::move(ParsedDependencies)));
        Known.emplace(IdText, std::move(AssetId));
    }
    yyjson_doc_free(Document);
    return Valid && Out.size() == 1000 && OutEdges == 5000;
}

void SampleRss(FMetrics& Metrics)
{
    const auto Memory = Core::FPlatformMemory::QueryProcessMemory();
    if (Memory.bAvailable)
        Metrics.PeakRss = std::max(Metrics.PeakRss, Memory.ResidentBytes);
}

Asset::FAssetDigest AggregateDigest(
    const FAssetCookScheduleOutput& Output)
{
    Core::TArray<Core::uint8> Bytes;
    for (const auto& Result : Output.Results)
    {
        const auto Digest = Asset::FAssetDigest::FromBytes(Result.Artifact);
        const auto& Value = Digest.GetBytes();
        Bytes.insert(Bytes.end(), Value.begin(), Value.end());
    }
    return Asset::FAssetDigest::FromBytes(Bytes);
}

void WriteReport(
    const FAssetCookerBenchmarkOptions& Options,
    const FLimits& Limits,
    const FMetrics& Metrics,
    bool Passed)
{
    const std::filesystem::path Path(Options.Report);
    if (!Path.parent_path().empty())
        std::filesystem::create_directories(Path.parent_path());
    std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
    Out << std::setprecision(9)
        << "Feature 025 Asset Cooker performance gate\n"
        << "profile=" << (Options.CiProfile ? "ci-4x" : "apple-m4-pro-reference") << "\n"
        << "corpus=" << Options.Corpus << "\n"
        << "corpus_sha256=" << Metrics.CorpusDigest.ToLowerHex().CStr() << "\n"
        << "assets=" << Metrics.Assets << "\n"
        << "dependency_edges=" << Metrics.Edges << "\n"
        << "payload_bytes=" << Metrics.PayloadBytes << "\n"
        << "artifact_digest=" << Metrics.ArtifactDigest.ToLowerHex().CStr() << "\n"
        << "representative_generation="
        << Metrics.RepresentativeGeneration.ToLowerHex().CStr() << "\n"
        << "plan_seconds=" << Metrics.Plan << " limit=" << Limits.Plan << "\n"
        << "cached_seconds=" << Metrics.Cached << " limit=" << Limits.Cached << "\n"
        << "validate_seconds=" << Metrics.Validate << " limit=" << Limits.Validate << "\n"
        << "synthetic_clean_seconds=" << Metrics.Clean << " limit=" << Limits.Clean << "\n"
        << "representative_clean_seconds=" << Metrics.Representative
        << " limit=" << Limits.Representative << "\n"
        << "peak_rss_bytes=" << Metrics.PeakRss << " limit=" << Limits.PeakRss << "\n"
        << "passed=" << (Passed ? "true" : "false") << "\n";
}

} // namespace

FAssetCookerBenchmarkResult RunAssetCookerBenchmark(
    const FAssetCookerBenchmarkOptions& Options)
{
    FAssetCookerBenchmarkResult Result;
    if (!Options.Enabled)
    {
        Record(Result, true, "asset-cooker benchmark is opt-in");
        return Result;
    }

    FLimits Limits;
    if (Options.CiProfile)
    {
        Limits.Plan *= 4.0;
        Limits.Cached *= 4.0;
        Limits.Validate *= 4.0;
        Limits.Clean *= 4.0;
        Limits.Representative *= 4.0;
    }
    FMetrics Metrics;
    Core::TArray<Asset::FAssetImportOutput> Outputs;
    const bool Loaded = LoadCorpus(
        Options.Corpus, Outputs, Metrics.Edges, Metrics.CorpusDigest);
    Metrics.Assets = static_cast<Core::uint32>(Outputs.size());
    Record(Result, Loaded,
        "synthetic benchmark corpus has exactly 1000 assets and 5000 DAG edges");
    if (!Loaded) return Result;

    FAssetCookGraphPlan Plan;
    Asset::EAssetResult Planned = Asset::EAssetResult::ProcessingFailure;
    Metrics.Plan = Measure([&]
    {
        Planned = FAssetCookGraph::Build(
            Outputs, Asset::EAssetCookSelectionMode::CookAll, {}, {}, Plan);
    });
    SampleRss(Metrics);

    FAssetCookScheduleOutput Clean;
    Asset::EAssetResult Cleaned = Asset::EAssetResult::ProcessingFailure;
    Metrics.Clean = Measure([&]
    {
        Cleaned = FAssetCookScheduler::Execute(
            Plan, 8,
            [](const FAssetCookGraphNode& Node,
               const Core::TArray<FAssetCookScheduledResult>& Dependencies)
            {
                for (const Core::uint32 Dependency : Node.Dependencies)
                    if (Dependency >= Dependencies.size() ||
                        Dependencies[Dependency].Artifact.empty())
                        return FAssetCookScheduledResult{
                            Asset::EAssetResult::ProcessingFailure, {}};
                Core::TArray<Core::uint8> Artifact(256 + Node.PlanIndex % 17);
                std::fill(Artifact.begin(), Artifact.end(),
                    static_cast<Core::uint8>(Node.PlanIndex & 0xffU));
                return FAssetCookScheduledResult{
                    Asset::EAssetResult::Success, std::move(Artifact)};
            }, Clean);
    });
    for (const auto& Entry : Clean.Results)
        Metrics.PayloadBytes += Entry.Artifact.size();
    Metrics.ArtifactDigest = AggregateDigest(Clean);
    SampleRss(Metrics);

    FAssetCookScheduleOutput Cached;
    Asset::EAssetResult CachedResult = Asset::EAssetResult::ProcessingFailure;
    Metrics.Cached = Measure([&]
    {
        CachedResult = FAssetCookScheduler::Execute(
            Plan, 8,
            [&Clean](const FAssetCookGraphNode& Node,
                     const Core::TArray<FAssetCookScheduledResult>&)
            {
                return FAssetCookScheduledResult{
                    Asset::EAssetResult::Success,
                    Clean.Results[Node.PlanIndex].Artifact};
            }, Cached);
    });
    SampleRss(Metrics);

    bool Validated = true;
    Metrics.Validate = Measure([&]
    {
        Validated = Cached.Results.size() == Clean.Results.size();
        for (Core::usize Index = 0; Validated && Index < Clean.Results.size(); ++Index)
            Validated = Asset::FAssetDigest::FromBytes(Clean.Results[Index].Artifact) ==
                Asset::FAssetDigest::FromBytes(Cached.Results[Index].Artifact);
    });
    SampleRss(Metrics);

    const auto RepresentativeRoot = std::filesystem::temp_directory_path() /
        "stoner-asset-cooker-benchmark-representative";
    std::filesystem::remove_all(RepresentativeRoot);
    auto RepresentativeRequest = Request(
        RepresentativeRoot,
        "Tests/Fixtures/AssetCooker/Representative", 8);
    RepresentativeRequest.CachePolicy =
        Stoner::AssetCooker::EAssetCookCachePolicy::IgnoreExisting;
    Stoner::AssetCooker::FAssetCookReport RepresentativeReport;
    Stoner::AssetCooker::FAssetCookResult RepresentativeResult;
    Metrics.Representative = Measure([&]
    {
        RepresentativeResult = Stoner::AssetCooker::FAssetCookRunner::Run(
            RepresentativeRequest, RepresentativeReport);
    });
    Metrics.RepresentativeGeneration =
        RepresentativeResult.Manifest.GenerationId;
    SampleRss(Metrics);
    std::filesystem::remove_all(RepresentativeRoot);

    const bool ShapePassed = Planned == Asset::EAssetResult::Success &&
        Cleaned == Asset::EAssetResult::Success &&
        CachedResult == Asset::EAssetResult::Success && Validated &&
        RepresentativeResult.Succeeded() && Plan.Nodes.size() == 1000 &&
        Plan.DependencyEdges == 5000 && Clean.Results.size() == 1000 &&
        Metrics.ArtifactDigest == AggregateDigest(Cached);
    const bool TimePassed = Metrics.Plan <= Limits.Plan &&
        Metrics.Cached <= Limits.Cached && Metrics.Validate <= Limits.Validate &&
        Metrics.Clean <= Limits.Clean &&
        Metrics.Representative <= Limits.Representative;
    const bool MemoryPassed = Metrics.PeakRss != 0 &&
        Metrics.PeakRss < Limits.PeakRss;
    Record(Result, ShapePassed,
        "synthetic plan clean cached validation and representative clean are correct");
    Record(Result, TimePassed,
        "all selected benchmark profile time limits pass as hard gates");
    Record(Result, MemoryPassed,
        "synthetic benchmark peak process RSS remains below 1 GiB");
    const bool Passed = Result.Failed == 0;
    WriteReport(Options, Limits, Metrics, Passed);
    return Result;
}
