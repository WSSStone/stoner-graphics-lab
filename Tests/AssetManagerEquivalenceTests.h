#pragma once

struct FAssetManagerEquivalenceTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerEquivalenceTestResult
RunAssetManagerEquivalenceTests();

