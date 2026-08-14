#pragma once

struct FAssetGLTFLimitTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetGLTFLimitTestResult RunAssetGLTFLimitTests();
