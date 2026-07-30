#pragma once

#include "AssetKTX2Tests.h"

struct FAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options = {});
