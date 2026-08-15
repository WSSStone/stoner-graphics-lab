#pragma once

struct FAssetManagerStressTestResult
{
    int Passed = 0;
    int Failed = 0;
};

struct FAssetManagerScaleWorkloadResult
{
    bool Passed = false;
    long long Milliseconds = 0;
};

[[nodiscard]] FAssetManagerStressTestResult RunAssetManagerStressTests();
[[nodiscard]] FAssetManagerScaleWorkloadResult
RunAssetManagerScaleWorkload();
