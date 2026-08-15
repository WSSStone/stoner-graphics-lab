#pragma once

#include <string>

struct FAssetManagerBenchmarkResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerBenchmarkResult RunAssetManagerBenchmark(
    bool Enabled,
    bool CiProfile,
    const std::string& ReportPath);
