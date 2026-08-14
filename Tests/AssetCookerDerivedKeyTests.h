#pragma once

struct FAssetCookerDerivedKeyTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerDerivedKeyTestResult RunAssetCookerDerivedKeyTests();
