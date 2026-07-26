#pragma once

struct FCorePlatformOwnershipTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCorePlatformOwnershipTestResult RunCorePlatformOwnershipTests();
