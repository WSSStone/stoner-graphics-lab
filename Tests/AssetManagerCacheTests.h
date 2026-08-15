#pragma once

struct FAssetManagerCacheTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerCacheTestResult RunAssetManagerCacheTests();
