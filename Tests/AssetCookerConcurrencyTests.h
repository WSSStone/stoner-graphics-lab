#pragma once

struct FAssetCookerConcurrencyTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerConcurrencyTestResult RunAssetCookerConcurrencyTests();
