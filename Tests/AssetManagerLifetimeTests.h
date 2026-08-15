#pragma once

struct FAssetManagerLifetimeTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerLifetimeTestResult RunAssetManagerLifetimeTests();
