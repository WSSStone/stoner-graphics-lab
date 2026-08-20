#pragma once

struct FCorePlatformProcessTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCorePlatformProcessTestResult RunCorePlatformProcessTests(
    const char* ProbeExecutable);
