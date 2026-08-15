#pragma once

struct FAssetManagerCompletionTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerCompletionTestResult
RunAssetManagerCompletionTests();
