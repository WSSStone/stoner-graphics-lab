#pragma once

struct FAssetManagerCancellationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerCancellationTestResult
RunAssetManagerCancellationTests();

