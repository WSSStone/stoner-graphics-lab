#pragma once

#include "Core/FString.h"

struct FAssetMaterialShaderTestOptions
{
    int DeterminismRuns = 20;
    Stoner::Core::FString ReportPath;
};

struct FAssetMaterialShaderTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetMaterialShaderTestResult RunAssetMaterialShaderTests(
    const FAssetMaterialShaderTestOptions& Options = {});
