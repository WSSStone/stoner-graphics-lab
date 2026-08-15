#pragma once

struct FAssetManagerGenerationLeaseTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerGenerationLeaseTestResult
RunAssetManagerGenerationLeaseTests();
