#include "AssetStaticModelBenchmark.h"

#include "AssetTests.h"
#include "StaticModelTestSupport.h"

#include "Asset/FMaterialAsset.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FTextureAsset.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

struct FMeasuredImport
{
    EAssetResult Result = EAssetResult::ProcessingFailure;
    double Seconds = 0.0;
    FStaticModelImportStatistics Statistics;
    TArray<FAssetImportOutput> Outputs;
};

std::string PlatformName()
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "Linux";
#endif
}

std::string BuildProfile()
{
#if defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

std::string CompilerName()
{
#if defined(__clang__)
    return std::string("Clang ") + __clang_version__;
#elif defined(_MSC_VER)
    return std::string("MSVC ") + std::to_string(_MSC_VER);
#elif defined(__GNUC__)
    return std::string("GCC ") + __VERSION__;
#else
    return "unknown";
#endif
}

std::string HostValue(const char* Name)
{
#if defined(__APPLE__)
    size_t Size = 0;
    if (sysctlbyname(Name, nullptr, &Size, nullptr, 0) != 0 || Size == 0)
        return "unknown";
    std::string Value(Size, '\0');
    if (sysctlbyname(Name, Value.data(), &Size, nullptr, 0) != 0)
        return "unknown";
    while (!Value.empty() && Value.back() == '\0') Value.pop_back();
    return Value;
#else
    (void)Name;
    return "reported-by-cross-platform-ci";
#endif
}

FMeasuredImport Measure(const std::filesystem::path& Path)
{
    FMeasuredImport Capture;
    FStaticModelImportRequest Request;
    Request.AssetRequest = MakeRequest(Path);
    Request.Profile = std::dynamic_pointer_cast<const FStaticModelImportProfile>(
        Request.AssetRequest.Parameters);
    Request.DependencyResolver = MakeShared<FFixtureResolver>();
    Request.Statistics = &Capture.Statistics;
    const auto Start = std::chrono::steady_clock::now();
    Capture.Result = ImportStaticModel(Request, Capture.Outputs);
    Capture.Seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - Start).count();
    return Capture;
}

bool HasRepresentativeShape(const FMeasuredImport& Import)
{
    uint64 Vertices = 0;
    uint64 Indices = 0;
    uint64 Primitives = 0;
    uint64 Materials = 0;
    uint64 Textures = 0;
    for (const auto& Output : Import.Outputs)
    {
        if (const auto Mesh = std::dynamic_pointer_cast<const FStaticMeshAsset>(
                Output.Payload))
        {
            Primitives += Mesh->GetDesc().Primitives.size();
            for (const auto& Primitive : Mesh->GetDesc().Primitives)
            {
                Vertices += Primitive.Vertices.Positions.size();
                Indices += Primitive.Indices.GetIndexCount();
            }
        }
        Materials += std::dynamic_pointer_cast<const FMaterialAsset>(
            Output.Payload) ? 1 : 0;
        Textures += std::dynamic_pointer_cast<const FTextureAsset>(
            Output.Payload) ? 1 : 0;
    }
    return Vertices >= 100000 && Indices >= 300000 && Primitives >= 16 &&
        Materials + Textures >= 16;
}

void WriteReport(
    const FAssetStaticModelTestOptions& Options,
    const FMeasuredImport& Warmup,
    const TArray<FMeasuredImport>& Runs,
    bool Passed)
{
    std::filesystem::create_directories("Validation/024/reports");
    std::ofstream Report("Validation/024/reports/performance.json",
                         std::ios::binary | std::ios::trunc);
    const FAssetDigest FixtureDigest = FAssetDigest::FromBytes(
        ReadFixture(Options.PerformanceFixture));
    Report << std::setprecision(9)
           << "{\n  \"schema\": \"stoner.static-model.performance/v1\",\n"
           << "  \"reference_profile\": \"Apple M4 Pro macOS Release\",\n"
           << "  \"platform\": \"" << PlatformName() << "\",\n"
           << "  \"build_profile\": \"" << BuildProfile() << "\",\n"
           << "  \"compiler\": \"" << CompilerName() << "\",\n"
           << "  \"cpu_model\": \"" << HostValue("machdep.cpu.brand_string")
           << "\",\n"
           << "  \"os_version\": \"" << HostValue("kern.osproductversion")
           << "\",\n"
           << "  \"hardware_threads\": " << std::thread::hardware_concurrency()
           << ",\n"
           << "  \"fixture\": \"" << Options.PerformanceFixture << "\",\n"
           << "  \"fixture_sha256\": \""
           << FixtureDigest.ToLowerHex().CStr() << "\",\n"
           << "  \"warmup_runs\": 1,\n"
           << "  \"measured_runs\": " << Runs.size() << ",\n"
           << "  \"warmup_seconds\": " << Warmup.Seconds << ",\n"
           << "  \"max_seconds\": " << Options.PerformanceMaxSeconds << ",\n"
           << "  \"aggregate_allocation_limit_bytes\": "
           << FStaticModelImportLimits::DefaultAggregateDependencyBytes << ",\n"
           << "  \"measured\": [\n";
    for (usize Index = 0; Index < Runs.size(); ++Index)
    {
        const auto& Run = Runs[Index];
        Report << "    {\"seconds\": " << Run.Seconds
               << ", \"source_bytes\": " << Run.Statistics.SourceBytes
               << ", \"parser_peak_bytes\": "
               << Run.Statistics.ParserPeakBytes
               << ", \"dependency_bytes\": "
               << Run.Statistics.DependencyBytes
               << ", \"decoded_geometry_bytes\": "
               << Run.Statistics.DecodedGeometryBytes
               << ", \"tracked_request_owned_peak_bytes\": "
               << Run.Statistics.TrackedRequestOwnedPeakBytes << "}"
               << (Index + 1 == Runs.size() ? "\n" : ",\n");
    }
    Report << "  ],\n  \"passed\": " << (Passed ? "true" : "false") << "\n}\n";
}

void Record(FAssetStaticModelBenchmarkResult& Result, bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}
}

FAssetStaticModelBenchmarkResult
RunAssetStaticModelBenchmark(const FAssetStaticModelTestOptions& Options)
{
    FAssetStaticModelBenchmarkResult Result;
    if (Options.PerformanceRuns == 0)
    {
        Record(Result, true, "static-model performance gate is opt-in");
        return Result;
    }
    if (Options.PerformanceFixture.empty() ||
        !std::filesystem::is_regular_file(Options.PerformanceFixture))
    {
        Record(Result, false, "static-model performance fixture is available");
        return Result;
    }

    const FMeasuredImport Warmup = Measure(Options.PerformanceFixture);
    TArray<FMeasuredImport> Runs;
    Runs.reserve(static_cast<usize>(Options.PerformanceRuns));
    bool Passed = Warmup.Result == EAssetResult::Success &&
        HasRepresentativeShape(Warmup);
    for (int Index = 0; Index < Options.PerformanceRuns; ++Index)
    {
        Runs.push_back(Measure(Options.PerformanceFixture));
        const auto& Run = Runs.back();
        Passed = Passed && Run.Result == EAssetResult::Success &&
            HasRepresentativeShape(Run) &&
            Run.Seconds <= Options.PerformanceMaxSeconds &&
            Run.Statistics.TrackedRequestOwnedPeakBytes <=
                FStaticModelImportLimits::DefaultAggregateDependencyBytes;
    }
    WriteReport(Options, Warmup, Runs, Passed);
    Record(Result, Passed,
        "representative imports meet shape time and tracked-memory gates");
    return Result;
}
