#pragma once

struct FAssetStaticModelConcurrencyTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetStaticModelConcurrencyTestResult
RunAssetStaticModelConcurrencyTests();
