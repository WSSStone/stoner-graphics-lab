#pragma once

struct FAssetGLTFResolverTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetGLTFResolverTestResult RunAssetGLTFResolverTests();
