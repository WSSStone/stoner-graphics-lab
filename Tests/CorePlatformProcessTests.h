#pragma once

struct FCorePlatformProcessTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCorePlatformProcessTestResult RunCorePlatformProcessTests(
    const char* ProbeExecutable);

[[nodiscard]] FCorePlatformProcessTestResult RunCorePlatformTerminationTests(
    const char* TestExecutable);
[[noreturn]] void RunCorePlatformTerminationChild(int ExitCode);
