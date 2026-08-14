#pragma once

struct FAssetCookerIncrementalTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerIncrementalTestResult RunAssetCookerIncrementalTests();
