#include "TriangleDemoIntegrationTests.h"

#include "FDemoConfiguration.h"
#include "FStonerDemoApplication.h"
#include "FDemoValidationMonitor.h"

#include <iostream>
#include <filesystem>
#include <string_view>

namespace
{
using namespace Stoner::Demo;

void Record(FTriangleDemoIntegrationTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed) { ++Result.Passed; std::cout << "[PASS] " << Name << '\n'; }
    else { ++Result.Failed; std::cout << "[FAIL] " << Name << '\n'; }
}

EDemoExitCode Parse(std::initializer_list<const char*> Arguments, FDemoConfiguration& Out, Stoner::Core::FString& Reason)
{
    Stoner::Core::TArray<const char*> Values(Arguments);
    return FDemoConfiguration::Parse(static_cast<int>(Values.size()), Values.data(), Out, Reason);
}

void TestConfiguration(FTriangleDemoIntegrationTestResult& Result)
{
    FDemoConfiguration Config;
    Stoner::Core::FString Reason;
    Record(Result, Parse({"StonerDemo", "--mode", "headless"}, Config, Reason) == EDemoExitCode::Success &&
        Config.FrameBudget == 4096 && Config.WarmupFrames == 512 && Config.MemorySampleInterval == 128 &&
        Config.MaxFramesInFlight == 2, "Triangle demo headless defaults match endurance profile");
    Record(Result, Parse({"StonerDemo", "--mode", "validate", "--frames", "0"}, Config, Reason) == EDemoExitCode::InvalidConfiguration,
        "Triangle demo rejects zero bounded frame budget");
    Record(Result, Parse({"StonerDemo", "--unknown", "value"}, Config, Reason) == EDemoExitCode::InvalidConfiguration,
        "Triangle demo rejects unknown options");
    Record(Result, Parse({"StonerDemo", "--mode", "headless", "--frames", "20", "--warmup-frames", "10", "--memory-sample-interval", "2"}, Config, Reason) == EDemoExitCode::InvalidConfiguration,
        "Triangle demo requires ten post-warmup memory samples");
    Record(Result, Parse({"StonerDemo", "--mode", "headless", "--frames", "40", "--warmup-frames", "0",
        "--memory-sample-interval", "4", "--max-memory-growth-mib", "32", "--max-memory-growth-percent", "7.5",
        "--width", "800", "--height", "600", "--frames-in-flight", "3", "--shader-dir", "Demo/StonerDemo/Shaders",
        "--validation-output", "Build/custom-report.txt", "--enable-validation"}, Config, Reason) == EDemoExitCode::Success &&
        Config.ClientWidth == 800 && Config.ClientHeight == 600 && Config.MaxFramesInFlight == 3 && Config.bEnableValidationLayers,
        "Triangle demo parses every canonical CLI option");
    Record(Result, Parse({"StonerDemo", "--mode", "headless", "--frames", "abc"}, Config, Reason) == EDemoExitCode::InvalidConfiguration &&
        Parse({"StonerDemo", "--width", "-1"}, Config, Reason) == EDemoExitCode::InvalidConfiguration &&
        Parse({"StonerDemo", "--max-memory-growth-percent", "nan"}, Config, Reason) == EDemoExitCode::InvalidConfiguration,
        "Triangle demo rejects malformed numeric values");
}

void TestDeterministicLifecycle(FTriangleDemoIntegrationTestResult& Result)
{
    FDemoConfiguration Config;
    Stoner::Core::FString Reason;
    const EDemoExitCode ParseResult = Parse({"StonerDemo", "--mode", "headless", "--frames", "20",
        "--warmup-frames", "0", "--memory-sample-interval", "2", "--validation-output",
        "Build/Mac/Debug/Tests/triangle-demo-test-report.txt"}, Config, Reason);
    FStonerDemoApplication App(Config);
    const EDemoExitCode RunResult = ParseResult == EDemoExitCode::Success ? App.Run() : ParseResult;
    Record(Result, RunResult == EDemoExitCode::Success && App.GetCompletedFrames() == 20 &&
        App.GetLifecycleState() == EDemoLifecycleState::Stopped,
        "Triangle demo deterministic lifecycle completes exact frame budget and shutdown");
    const Stoner::Core::FString StableText = App.GetDiagnostics().BuildStableText();
    const std::string_view Text = StableText.View();
    Record(Result, Text.find("stage=Runtime") != std::string_view::npos &&
        Text.find("stage=Shutdown") != std::string_view::npos && Text.find("0x") == std::string_view::npos,
        "Triangle demo diagnostics are stable and omit native addresses");
}

void TestNativeFallbackRejection(FTriangleDemoIntegrationTestResult& Result)
{
    FDemoConfiguration Config;
    Stoner::Core::FString Reason;
    Record(Result, Parse({"StonerDemo", "--mode", "interactive"}, Config, Reason) == EDemoExitCode::Success,
        "Triangle demo interactive configuration parses");
    FStonerDemoApplication App(Config);
    App.SetFailureInjection(EDemoStage::Runtime);
    Record(Result, App.Run() != EDemoExitCode::Success,
        "Triangle demo native-required mode never reports deterministic visible success");
}

void TestInitializationContractAndShaderStages(FTriangleDemoIntegrationTestResult& Result)
{
    FDemoConfiguration Config;
    Config.RunMode = EDemoRunMode::DeterministicHeadless;
    Config.FrameBudget = 20;
    Config.WarmupFrames = 0;
    Config.MemorySampleInterval = 2;
    Config.MaxMemoryGrowthBytes = 1024 * 1024;
    Config.MaxMemoryGrowthPercent = 5.0;
    FStonerDemoApplication Initialized(Config);
    const EDemoExitCode InitResult = Initialized.Initialize();
    const std::string Stable = Initialized.GetDiagnostics().BuildStableText().ToStdString();
    const std::size_t WindowAt = Stable.find("stage=Window");
    const std::size_t RuntimeAt = Stable.find("stage=Runtime");
    const std::size_t ShaderAt = Stable.find("stage=Shader");
    const std::size_t UploadAt = Stable.find("stage=Upload");
    const std::size_t PipelineAt = Stable.find("stage=Pipeline");
    Record(Result, InitResult == EDemoExitCode::Success && Initialized.GetFrameContextCount() == 2 &&
            WindowAt < RuntimeAt && RuntimeAt < ShaderAt && ShaderAt < UploadAt && UploadAt < PipelineAt,
        "Triangle demo initializes window runtime shaders upload pipeline and two frame slots in contract order");
    Record(Result, Initialized.Shutdown() == EDemoExitCode::Success &&
            Initialized.GetLifecycleState() == EDemoLifecycleState::Stopped,
        "Triangle demo normal shutdown remains idempotent after initialized state");

    const std::filesystem::path InvalidDirectory = "Build/Tests/invalid-stage-shaders";
    std::error_code Error;
    std::filesystem::create_directories(InvalidDirectory, Error);
    std::filesystem::copy_file("Demo/StonerDemo/Shaders/Triangle.frag.spv", InvalidDirectory / "Triangle.vert.spv",
        std::filesystem::copy_options::overwrite_existing, Error);
    Error.clear();
    std::filesystem::copy_file("Demo/StonerDemo/Shaders/Triangle.vert.spv", InvalidDirectory / "Triangle.frag.spv",
        std::filesystem::copy_options::overwrite_existing, Error);
    FDemoConfiguration Invalid = Config;
    Invalid.ShaderDirectory = InvalidDirectory.string().c_str();
    FStonerDemoApplication WrongStages(Invalid);
    Record(Result, WrongStages.Initialize() == EDemoExitCode::InitializationFailed &&
            WrongStages.GetDiagnostics().GetPrimaryExitCode() == EDemoExitCode::InitializationFailed &&
            WrongStages.GetLifecycleState() == EDemoLifecycleState::Stopped &&
            WrongStages.Shutdown() == EDemoExitCode::Success,
        "Triangle demo rejects swapped shader stages and cleans partial initialization");
}

void TestPresentationRecovery(FTriangleDemoIntegrationTestResult& Result)
{
    FDemoConfiguration Config;
    Config.RunMode = EDemoRunMode::DeterministicHeadless;
    Config.FrameBudget = 20;
    Config.WarmupFrames = 0;
    Config.MemorySampleInterval = 2;
    Config.MaxMemoryGrowthBytes = 1024;
    Config.MaxMemoryGrowthPercent = 5.0;
    FStonerDemoApplication App(Config);
    Record(Result, App.Initialize() == EDemoExitCode::Success, "Triangle demo recovery fixture initializes");
    bool bRecovered = true;
    for (int Cycle = 0; Cycle < 20; ++Cycle)
    {
        const double Start = static_cast<double>(Cycle) * 3000.0;
        bRecovered = bRecovered && App.NotifyDrawableExtent(0, 0, Start) == EDemoExitCode::Success &&
            App.GetLifecycleState() == EDemoLifecycleState::PresentationPaused &&
            App.NotifyDrawableExtent(1280, 720, Start + 10.0) == EDemoExitCode::Success &&
            App.NotifyPresentSuccess(Start + 2010.0) == EDemoExitCode::Success;
    }
    Record(Result, bRecovered && App.GetPresentationGeneration() == 20 &&
        App.GetRecoveryDurationsMilliseconds().size() == 20,
        "Triangle demo recovers twenty presentation generations at exact 2000ms boundary");
    (void)App.NotifyDrawableExtent(0, 0, 70000.0);
    (void)App.NotifyDrawableExtent(1280, 720, 70010.0);
    Record(Result, App.NotifyPresentSuccess(72011.0) == EDemoExitCode::ValidationFailed,
        "Triangle demo rejects presentation recovery beyond 2000ms boundary");
    (void)App.Shutdown();
}

void TestFailureInjectionAndFirstFailure(FTriangleDemoIntegrationTestResult& Result)
{
    const EDemoStage Stages[] = {EDemoStage::Window, EDemoStage::Runtime, EDemoStage::Shader,
        EDemoStage::Upload, EDemoStage::Pipeline, EDemoStage::Acquire, EDemoStage::Record,
        EDemoStage::Submit, EDemoStage::Present, EDemoStage::Memory, EDemoStage::Report, EDemoStage::Shutdown};
    bool bAllOwned = true;
    for (EDemoStage Stage : Stages)
    {
        FDemoConfiguration Config;
        Config.RunMode = EDemoRunMode::DeterministicHeadless;
        Config.FrameBudget = 20;
        Config.WarmupFrames = 0;
        Config.MemorySampleInterval = 2;
        Config.MaxMemoryGrowthBytes = 1024 * 1024;
        Config.MaxMemoryGrowthPercent = 5.0;
        Config.ValidationOutputPath = "Build/Mac/Debug/Tests/failure-report.txt";
        FStonerDemoApplication App(Config);
        App.SetFailureInjection(Stage);
        const EDemoExitCode Exit = App.Run();
        const auto& Records = App.GetDiagnostics().GetRecords();
        const FDemoDiagnostic* FirstError = nullptr;
        for (const FDemoDiagnostic& Diagnostic : Records)
            if (Diagnostic.Result != EDemoExitCode::Success) { FirstError = &Diagnostic; break; }
        bAllOwned = bAllOwned && Exit != EDemoExitCode::Success && FirstError != nullptr &&
            FirstError->Stage == Stage && FirstError->Result == Exit;
    }
    Record(Result, bAllOwned, "Triangle demo injected failures preserve first-stage exit ownership");
}

void TestValidationMonitorBoundaries(FTriangleDemoIntegrationTestResult& Result)
{
    FDemoConfiguration Config;
    Config.RunMode = EDemoRunMode::DeterministicHeadless;
    Config.FrameBudget = 20;
    Config.WarmupFrames = 0;
    Config.MemorySampleInterval = 2;
    Config.MaxMemoryGrowthBytes = 100;
    Config.MaxMemoryGrowthPercent = 1.0;
    FDemoValidationMonitor Passing(Config);
    for (Stoner::Core::uint32 Index = 1; Index <= 10; ++Index) Passing.AddSyntheticSample(Index * 2, 1000 + (Index > 5 ? 100 : 0), {});
    Passing.SetRequestedFrames(20); Passing.SetCompletedFrames(20); Passing.SetRuntimeSnapshot({});
    Record(Result, Passing.Evaluate() && Passing.GetBaselineMedianBytes() == 1000 && Passing.GetFinalMedianBytes() == 1100,
        "Triangle demo validation monitor accepts exact absolute growth boundary");

    FDemoValidationMonitor Failing(Config);
    for (Stoner::Core::uint32 Index = 1; Index <= 10; ++Index) Failing.AddSyntheticSample(Index * 2, 1000 + (Index > 5 ? 101 : 0), {});
    Failing.SetRequestedFrames(20); Failing.SetCompletedFrames(20); Failing.SetRuntimeSnapshot({});
    Record(Result, !Failing.Evaluate(), "Triangle demo validation monitor rejects growth above configured boundary");

    Config.RunMode = EDemoRunMode::BoundedNative;
    FDemoValidationMonitor Visible(Config);
    for (Stoner::Core::uint32 Index = 1; Index <= 10; ++Index) Visible.AddSyntheticSample(Index * 2, 1000, {});
    Visible.SetRequestedFrames(20); Visible.SetCompletedFrames(20); Visible.SetRuntimeSnapshot({});
    Visible.SetFirstPresentMilliseconds(5000.0);
    for (int Index = 0; Index < 20; ++Index) Visible.AddRecoveryMilliseconds(2000.0);
    Record(Result, Visible.Evaluate(), "Triangle demo visible timing gates accept exact first-present and recovery boundaries");

    FDemoValidationMonitor AdditionalRecoveries(Config);
    for (Stoner::Core::uint32 Index = 1; Index <= 10; ++Index) AdditionalRecoveries.AddSyntheticSample(Index * 2, 1000, {});
    AdditionalRecoveries.SetRequestedFrames(20); AdditionalRecoveries.SetCompletedFrames(20); AdditionalRecoveries.SetRuntimeSnapshot({});
    AdditionalRecoveries.SetFirstPresentMilliseconds(5000.0);
    for (int Index = 0; Index < 31; ++Index) AdditionalRecoveries.AddRecoveryMilliseconds(1999.0);
    Record(Result, AdditionalRecoveries.Evaluate(),
        "Triangle demo visible timing gates accept more than twenty valid recoveries");

    FDemoValidationMonitor AdditionalSlowRecovery(Config);
    for (Stoner::Core::uint32 Index = 1; Index <= 10; ++Index) AdditionalSlowRecovery.AddSyntheticSample(Index * 2, 1000, {});
    AdditionalSlowRecovery.SetRequestedFrames(20); AdditionalSlowRecovery.SetCompletedFrames(20); AdditionalSlowRecovery.SetRuntimeSnapshot({});
    AdditionalSlowRecovery.SetFirstPresentMilliseconds(5000.0);
    for (int Index = 0; Index < 30; ++Index) AdditionalSlowRecovery.AddRecoveryMilliseconds(1999.0);
    AdditionalSlowRecovery.AddRecoveryMilliseconds(2000.001);
    Record(Result, !AdditionalSlowRecovery.Evaluate(),
        "Triangle demo visible timing gates reject a slow additional recovery");

    FDemoValidationMonitor SlowVisible(Config);
    for (Stoner::Core::uint32 Index = 1; Index <= 10; ++Index) SlowVisible.AddSyntheticSample(Index * 2, 1000, {});
    SlowVisible.SetRequestedFrames(20); SlowVisible.SetCompletedFrames(20); SlowVisible.SetRuntimeSnapshot({});
    SlowVisible.SetFirstPresentMilliseconds(5000.001);
    for (int Index = 0; Index < 20; ++Index) SlowVisible.AddRecoveryMilliseconds(2000.0);
    Record(Result, !SlowVisible.Evaluate(), "Triangle demo visible timing gates reject first present above 5000ms");
}

void TestStableDiagnosticsAndReports(FTriangleDemoIntegrationTestResult& Result)
{
    FDemoConfiguration Config;
    Config.RunMode = EDemoRunMode::DeterministicHeadless;
    Config.FrameBudget = 20; Config.WarmupFrames = 0; Config.MemorySampleInterval = 2;
    Config.MaxMemoryGrowthBytes = 1024; Config.MaxMemoryGrowthPercent = 5.0;
    Stoner::Core::FString FirstDiagnostics;
    bool bStableDiagnostics = true;
    for (int Run = 0; Run < 20; ++Run)
    {
        FStonerDemoApplication App(Config);
        App.SetFailureInjection(EDemoStage::Acquire);
        (void)App.Run();
        const Stoner::Core::FString Text = App.GetDiagnostics().BuildStableText();
        if (Run == 0) FirstDiagnostics = Text;
        else bStableDiagnostics = bStableDiagnostics && Text == FirstDiagnostics;
    }
    Record(Result, bStableDiagnostics, "Triangle demo normalized failure diagnostics are byte-stable across twenty runs");

    FDemoDiagnostics Diagnostics;
    Diagnostics.Add(EDemoStage::Runtime, EDemoExitCode::Success, "Runtime", "headless");
    Stoner::Core::FString FirstReport;
    bool bStableReports = true;
    for (int Run = 0; Run < 20; ++Run)
    {
        FDemoValidationMonitor Monitor(Config);
        for (Stoner::Core::uint32 Index = 1; Index <= 10; ++Index) Monitor.AddSyntheticSample(Index * 2, 1000, {});
        Monitor.SetRequestedFrames(20); Monitor.SetCompletedFrames(20); Monitor.SetRuntimeSnapshot({});
        (void)Monitor.Evaluate();
        const Stoner::Core::FString Text = Monitor.BuildReport(Diagnostics);
        if (Run == 0) FirstReport = Text;
        else bStableReports = bStableReports && Text == FirstReport;
    }
    Record(Result, bStableReports, "Triangle demo validation reports are byte-stable across twenty synthetic runs");

    Config.ValidationOutputPath = "SConstruct/report.txt";
    FStonerDemoApplication WriteFailure(Config);
    Record(Result, WriteFailure.Run() == EDemoExitCode::ReportFailed,
        "Triangle demo maps an unwritable validation output path to exit code seven");
}

} // namespace

FTriangleDemoIntegrationTestResult RunTriangleDemoIntegrationTests()
{
    FTriangleDemoIntegrationTestResult Result;
    TestConfiguration(Result);
    TestDeterministicLifecycle(Result);
    TestNativeFallbackRejection(Result);
    TestInitializationContractAndShaderStages(Result);
    TestPresentationRecovery(Result);
    TestFailureInjectionAndFirstFailure(Result);
    TestValidationMonitorBoundaries(Result);
    TestStableDiagnosticsAndReports(Result);
    return Result;
}
