#pragma once

struct FAssetCookerProfileTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerProfileTestResult RunAssetCookerProfileTests();
