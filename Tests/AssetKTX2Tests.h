#pragma once

#include <string>

struct FAssetKTX2TestOptions
{
    std::string EmitDirectory;
    std::string EmitSourceDirectory;
    std::string ReportPath;
    int DeterminismRuns = 1;
};

struct FAssetKTX2TestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetKTX2TestResult RunAssetKTX2Tests(
    const FAssetKTX2TestOptions& Options = {});
[[nodiscard]] FAssetKTX2TestResult RunAssetKTX2EncoderTests();
