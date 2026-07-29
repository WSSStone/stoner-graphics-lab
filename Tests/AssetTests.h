#pragma once

struct FAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetTestResult RunAssetTests();
