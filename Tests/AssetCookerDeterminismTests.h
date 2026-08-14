#pragma once

struct FAssetCookerDeterminismTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerDeterminismTestResult RunAssetCookerDeterminismTests();
