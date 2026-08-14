#pragma once

struct FAssetCookerTargetProfileTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerTargetProfileTestResult RunAssetCookerTargetProfileTests();
