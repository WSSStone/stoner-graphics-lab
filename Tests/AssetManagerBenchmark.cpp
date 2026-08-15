#include "AssetManagerBenchmark.h"

#include "AssetManagerStressTests.h"
#include "AssetManagerTestSupport.h"
#include "FAssetCompletionQueue.h"
#include "FAssetRequestTable.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;

struct FMetrics
{
    double AdmissionMilliseconds = 0.0;
    double GraphMilliseconds = 0.0;
    double PumpMilliseconds = 0.0;
    double LifecycleMilliseconds = 0.0;
    bool bAdmissionValid = false;
    bool bGraphValid = false;
    bool bPumpValid = false;
    bool bLifecycleValid = false;
};

double Milliseconds(
    std::chrono::steady_clock::time_point Begin,
    std::chrono::steady_clock::time_point End)
{
    return std::chrono::duration<double, std::milli>(End - Begin).count();
}

void Record(
    FAssetManagerBenchmarkResult& Result,
    bool Passed,
    const char* Name,
    double Value,
    double Limit)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name
              << " milliseconds=" << Value << " limit=" << Limit << '\n';
}

FMetrics Measure()
{
    FMetrics Result;
    const FAssetId Id = MakeRuntimeTestId("Runtime/Benchmark");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    auto Config = MakeDevelopmentManagerConfig(Extensions);
    Config.WorkerCount = 4;
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    Result.bAdmissionValid = FAssetManager::Create(
        Config, Manager, Diagnostics) == EAssetResult::Success;
    std::array<FAssetRequestHandle, 8> Requests;
    const auto AdmissionBegin = std::chrono::steady_clock::now();
    for (auto& Request : Requests)
        Result.bAdmissionValid = Result.bAdmissionValid &&
            Manager->Request<FRuntimeTestPayload>(Id, Request) ==
                EAssetResult::Success;
    Result.AdmissionMilliseconds = Milliseconds(
        AdmissionBegin, std::chrono::steady_clock::now());
    for (const auto Request : Requests)
    {
        FAssetRequestSnapshot Snapshot;
        Result.bAdmissionValid = Result.bAdmissionValid &&
            WaitForRequestTerminal(*Manager, Request, Snapshot) &&
            Snapshot.State == EAssetRequestState::Ready;
        (void)Manager->ReleaseRequest(Request);
    }
    (void)Manager->Shutdown();

    const auto Scale = RunAssetManagerScaleWorkload();
    Result.GraphMilliseconds = static_cast<double>(Scale.Milliseconds);
    Result.bGraphValid = Scale.Passed;

    constexpr Core::uint32 CompletionCount = 10000;
    FAssetCompletionQueue Queue(CompletionCount);
    FAssetRequestTable RequestTable(0x2600BEEFULL, 1);
    FAssetRequestHandle CompletionRequest;
    Result.bPumpValid = RequestTable.Allocate(CompletionRequest);
    for (Core::uint32 Index = 0; Index < CompletionCount; ++Index)
        Result.bPumpValid = Result.bPumpValid && Queue.Reserve() &&
            Queue.Enqueue(Index + 1, CompletionRequest,
                EAssetResult::Success,
                [](FAssetRequestHandle, EAssetResult) {});
    const auto PumpBegin = std::chrono::steady_clock::now();
    const FAssetPumpResult Pump = Queue.Pump(CompletionCount);
    Result.PumpMilliseconds = Milliseconds(
        PumpBegin, std::chrono::steady_clock::now());
    Result.bPumpValid = Result.bPumpValid &&
        Pump.Result == EAssetResult::Success &&
        Pump.Dispatched == CompletionCount && Queue.Reserved() == 0;

    auto LifecycleExtensions = MakeRuntimeTestExtensions(
        MakeRuntimeTestId("Runtime/BenchmarkLifecycle"));
    const FAssetId LifecycleId =
        MakeRuntimeTestId("Runtime/BenchmarkLifecycle");
    Core::TSharedPtr<FAssetManager> LifecycleManager;
    Diagnostics.clear();
    Result.bLifecycleValid = FAssetManager::Create(
        MakeDevelopmentManagerConfig(LifecycleExtensions),
        LifecycleManager, Diagnostics) == EAssetResult::Success;
    constexpr int LifecycleIterations = 10000;
    const auto LifecycleBegin = std::chrono::steady_clock::now();
    for (int Iteration = 0;
         Iteration < LifecycleIterations && Result.bLifecycleValid;
         ++Iteration)
    {
        FAssetRequestHandle Request;
        FAssetRequestSnapshot Snapshot;
        Result.bLifecycleValid = LifecycleManager->Request<FRuntimeTestPayload>(
            LifecycleId, Request) == EAssetResult::Success;
        for (int Poll = 0; Poll < 10000 && Result.bLifecycleValid; ++Poll)
        {
            Result.bLifecycleValid = LifecycleManager->Query(
                Request, Snapshot) == EAssetResult::Success;
            if (Snapshot.State == EAssetRequestState::Ready ||
                Snapshot.State == EAssetRequestState::Failed ||
                Snapshot.State == EAssetRequestState::Cancelled)
                break;
            std::this_thread::yield();
        }
        Result.bLifecycleValid = Result.bLifecycleValid &&
            Snapshot.State == EAssetRequestState::Ready &&
            LifecycleManager->ReleaseRequest(Request) == EAssetResult::Success;
    }
    Result.LifecycleMilliseconds = Milliseconds(
        LifecycleBegin, std::chrono::steady_clock::now());
    const auto LifecycleInspection = LifecycleManager->Inspect();
    Result.bLifecycleValid = Result.bLifecycleValid &&
        LifecycleInspection.CachedAssets == 0 &&
        LifecycleInspection.ActiveOperations == 0 &&
        LifecycleInspection.RequestRetentions == 0;
    (void)LifecycleManager->Shutdown();
    return Result;
}

bool WriteReport(
    const std::string& Path,
    bool CiProfile,
    const FMetrics& Metrics)
{
    if (Path.empty()) return true;
    std::error_code Error;
    const std::filesystem::path Output(Path);
    if (!Output.parent_path().empty())
        std::filesystem::create_directories(Output.parent_path(), Error);
    if (Error) return false;
    std::ofstream Stream(Output, std::ios::binary | std::ios::trunc);
    if (!Stream) return false;
    Stream << std::fixed << std::setprecision(3)
           << "feature=026-runtime-asset-manager\n"
           << "profile=" << (CiProfile ? "ci" : "reference") << '\n'
           << "reference_host=Apple-M4-Pro\n"
           << "graph_nodes=1000\n"
           << "graph_edges=5000\n"
           << "completion_count=10000\n"
           << "lifecycle_iterations=10000\n"
           << "admission_ms=" << Metrics.AdmissionMilliseconds << '\n'
           << "graph_ms=" << Metrics.GraphMilliseconds << '\n'
           << "pump_ms=" << Metrics.PumpMilliseconds << '\n'
           << "lifecycle_ms=" << Metrics.LifecycleMilliseconds << '\n';
    return static_cast<bool>(Stream);
}
} // namespace

FAssetManagerBenchmarkResult RunAssetManagerBenchmark(
    bool Enabled,
    bool CiProfile,
    const std::string& ReportPath)
{
    FAssetManagerBenchmarkResult Result;
    if (!Enabled) return Result;
    const FMetrics Metrics = Measure();
    const double Multiplier = CiProfile ? 4.0 : 1.0;
    Record(Result,
        Metrics.bAdmissionValid &&
            Metrics.AdmissionMilliseconds <= 5.0 * Multiplier,
        "eight-request admission benchmark", Metrics.AdmissionMilliseconds,
        5.0 * Multiplier);
    Record(Result,
        Metrics.bGraphValid &&
            Metrics.GraphMilliseconds <= 2000.0 * Multiplier,
        "pre-bound graph benchmark", Metrics.GraphMilliseconds,
        2000.0 * Multiplier);
    Record(Result,
        Metrics.bPumpValid && Metrics.PumpMilliseconds <= 50.0 * Multiplier,
        "pre-reserved completion pump benchmark", Metrics.PumpMilliseconds,
        50.0 * Multiplier);
    Record(Result,
        Metrics.bLifecycleValid &&
            Metrics.LifecycleMilliseconds <= 30000.0 * Multiplier,
        "ten-thousand lifecycle benchmark", Metrics.LifecycleMilliseconds,
        30000.0 * Multiplier);
    Record(Result, WriteReport(ReportPath, CiProfile, Metrics),
        "benchmark report written", 0.0, 0.0);
    return Result;
}
