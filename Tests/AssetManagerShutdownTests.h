#pragma once

struct FAssetManagerShutdownTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerShutdownTestResult RunAssetManagerShutdownTests();
