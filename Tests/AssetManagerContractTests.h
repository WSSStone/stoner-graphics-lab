#pragma once

struct FAssetManagerContractTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerContractTestResult RunAssetManagerContractTests();

