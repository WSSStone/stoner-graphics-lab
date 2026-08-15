#pragma once

struct FAssetManagerCookedTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerCookedTestResult RunAssetManagerCookedTests();
