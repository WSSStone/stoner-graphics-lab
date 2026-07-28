#pragma once

struct FAssetCoreTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCoreTestResult RunAssetCoreTests();
