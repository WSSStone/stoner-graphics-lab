#pragma once

struct FAssetCookerSourceCatalogTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerSourceCatalogTestResult
RunAssetCookerSourceCatalogTests();
