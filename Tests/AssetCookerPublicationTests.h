#pragma once

struct FAssetCookerPublicationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerPublicationTestResult RunAssetCookerPublicationTests();
