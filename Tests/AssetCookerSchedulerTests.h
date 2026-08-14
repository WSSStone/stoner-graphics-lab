#pragma once

struct FAssetCookerSchedulerTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerSchedulerTestResult RunAssetCookerSchedulerTests();
