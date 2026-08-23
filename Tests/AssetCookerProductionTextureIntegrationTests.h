#pragma once

struct FAssetCookerProductionTextureIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerProductionTextureIntegrationTestResult
RunAssetCookerProductionTextureIntegrationTests();
