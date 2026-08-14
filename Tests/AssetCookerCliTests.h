#pragma once

struct FAssetCookerCliTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerCliTestResult RunAssetCookerCliTests(
    const char* AssetCookerExecutable);
