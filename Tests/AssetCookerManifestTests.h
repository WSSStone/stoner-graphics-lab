#pragma once

struct FAssetCookerManifestTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerManifestTestResult RunAssetCookerManifestTests();
