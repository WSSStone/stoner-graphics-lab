#pragma once

struct FAssetGLTFPolicyTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetGLTFPolicyTestResult RunAssetGLTFPolicyTests();
