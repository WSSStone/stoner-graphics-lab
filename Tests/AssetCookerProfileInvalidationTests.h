#pragma once

struct FAssetCookerProfileInvalidationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerProfileInvalidationTestResult
RunAssetCookerProfileInvalidationTests();
