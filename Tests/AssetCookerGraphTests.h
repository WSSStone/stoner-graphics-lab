#pragma once

struct FAssetCookerGraphTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerGraphTestResult RunAssetCookerGraphTests();
