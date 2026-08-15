#pragma once

struct FAssetManagerDevelopmentTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerDevelopmentTestResult
RunAssetManagerDevelopmentTests();

