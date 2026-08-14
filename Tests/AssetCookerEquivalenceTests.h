#pragma once

struct FAssetCookerEquivalenceTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerEquivalenceTestResult
RunAssetCookerEquivalenceTests();
