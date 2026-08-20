#include "CorePlatformProcessTests.h"

#include "Core/FPlatformProcess.h"
#include "Core/SGPlatform.h"

#include <chrono>
#include <iostream>
#include <string>

namespace
{

using namespace Stoner::Core;

void Record(
    FCorePlatformProcessTestResult& Result,
    bool Passed,
    const char* Name)
{
    if (Passed)
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

FProcessExecutionRequest RequestFor(const char* ProbeExecutable)
{
    FProcessExecutionRequest Request;
    Request.ExecutablePath = FString(ProbeExecutable);
    Request.Limits.TimeoutMilliseconds = 2000;
    return Request;
}

} // namespace

FCorePlatformProcessTestResult RunCorePlatformProcessTests(
    const char* ProbeExecutable)
{
    FCorePlatformProcessTestResult Result;

    auto Utf8Request = RequestFor(ProbeExecutable);
    const FString Utf8Text("utf8-\xE4\xB8\xAD\xE6\x96\x87-\xE2\x9C\x93");
    Utf8Request.Arguments = {FString("--stdout"), Utf8Text};
    const auto Utf8Result = FPlatformProcess::Execute(Utf8Request);
    Record(
        Result,
        Utf8Result.Succeeded() && Utf8Result.StandardOutput == Utf8Text,
        "Explicit executable and UTF-8 argv round-trip without a shell");

    auto BoundedRequest = RequestFor(ProbeExecutable);
    BoundedRequest.Arguments = {
        FString("--repeat-stdout"), FString("128"),
        FString("--repeat-stderr"), FString("128")};
    BoundedRequest.Limits.MaxStdoutBytes = 31;
    BoundedRequest.Limits.MaxStderrBytes = 29;
    const auto BoundedResult = FPlatformProcess::Execute(BoundedRequest);
    Record(
        Result,
        BoundedResult.Succeeded() &&
            BoundedResult.StandardOutput.View().size() == 31 &&
            BoundedResult.StandardError.View().size() == 29 &&
            BoundedResult.bStdoutTruncated &&
            BoundedResult.bStderrTruncated,
        "Stdout and stderr capture remains bounded while pipes are drained");

    auto ExitRequest = RequestFor(ProbeExecutable);
    ExitRequest.Arguments = {FString("--exit"), FString("23")};
    const auto ExitResult = FPlatformProcess::Execute(ExitRequest);
    Record(
        Result,
        ExitResult.Status == EProcessExecutionStatus::Completed &&
            ExitResult.ExitCode == 23 && !ExitResult.Succeeded(),
        "Non-zero child exit code is preserved");

    auto TimeoutRequest = RequestFor(ProbeExecutable);
    TimeoutRequest.Arguments = {FString("--sleep-ms"), FString("500")};
    TimeoutRequest.Limits.TimeoutMilliseconds = 40;
    const auto Start = std::chrono::steady_clock::now();
    const auto TimeoutResult = FPlatformProcess::Execute(TimeoutRequest);
    const auto Elapsed = std::chrono::steady_clock::now() - Start;
    Record(
        Result,
        TimeoutResult.Status == EProcessExecutionStatus::TimedOut &&
            Elapsed < std::chrono::seconds(2),
        "Timeout terminates the child within a bounded interval");

    auto LiteralRequest = RequestFor(ProbeExecutable);
    const FString ShellText("; echo shell-expansion-must-not-run && $HOME");
    LiteralRequest.Arguments = {FString("--stdout"), ShellText};
    const auto LiteralResult = FPlatformProcess::Execute(LiteralRequest);
    Record(
        Result,
        LiteralResult.Succeeded() &&
            LiteralResult.StandardOutput == ShellText,
        "Shell metacharacters remain literal argv bytes");

    FProcessExecutionRequest MissingRequest;
#if SG_PLATFORM_WINDOWS
    MissingRequest.ExecutablePath = FString("C:/stoner/missing/process.exe");
#else
    MissingRequest.ExecutablePath = FString("/stoner/missing/process");
#endif
    const auto MissingResult = FPlatformProcess::Execute(MissingRequest);
    Record(
        Result,
        MissingResult.Status == EProcessExecutionStatus::LaunchFailed,
        "Missing explicit executable reports launch failure");

    FProcessExecutionRequest InvalidRequest;
    InvalidRequest.ExecutablePath = FString("search-path-command");
    const auto InvalidResult = FPlatformProcess::Execute(InvalidRequest);
    Record(
        Result,
        InvalidResult.Status == EProcessExecutionStatus::InvalidRequest,
        "Search-path executable names fail request validation");
    return Result;
}
