#pragma once

struct FAssetGLTFContainerTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetGLTFContainerTestResult RunAssetGLTFContainerTests();
