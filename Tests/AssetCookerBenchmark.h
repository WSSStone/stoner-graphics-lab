#pragma once

#include <string>

struct FAssetCookerBenchmarkOptions
{
    bool Enabled = false;
    bool CiProfile = false;
    std::string Corpus =
        "Tests/Fixtures/AssetCooker/Scale/scale-1000-5000.json";
    std::string Report = "Validation/025/reports/performance-m4-pro.txt";
};

struct FAssetCookerBenchmarkResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerBenchmarkResult RunAssetCookerBenchmark(
    const FAssetCookerBenchmarkOptions& Options);
