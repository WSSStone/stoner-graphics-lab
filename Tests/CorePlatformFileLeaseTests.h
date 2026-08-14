#pragma once

struct FCorePlatformFileLeaseTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCorePlatformFileLeaseTestResult
RunCorePlatformFileLeaseTests(const char* ProbeExecutable);
