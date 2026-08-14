#pragma once

struct FAssetStaticModelTestOptions;

struct FAssetStaticModelDeterminismTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetStaticModelDeterminismTestResult
RunAssetStaticModelDeterminismTests(const FAssetStaticModelTestOptions& Options);
