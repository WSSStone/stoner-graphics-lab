#pragma once

struct FAssetCookerPublishedValidationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerPublishedValidationTestResult
RunAssetCookerPublishedValidationTests();
