#pragma once

struct FCorePlatformTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCorePlatformTestResult RunCorePlatformTests();
