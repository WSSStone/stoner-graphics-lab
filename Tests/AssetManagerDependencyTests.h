#pragma once

struct FAssetManagerDependencyTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerDependencyTestResult
RunAssetManagerDependencyTests();

