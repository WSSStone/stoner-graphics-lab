#pragma once

struct FAssetManagerKernelTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerKernelTestResult RunAssetManagerKernelTests();
