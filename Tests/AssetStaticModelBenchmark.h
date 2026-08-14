#pragma once

struct FAssetStaticModelTestOptions;

struct FAssetStaticModelBenchmarkResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetStaticModelBenchmarkResult
RunAssetStaticModelBenchmark(const FAssetStaticModelTestOptions& Options);
