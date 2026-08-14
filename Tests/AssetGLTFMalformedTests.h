#pragma once

struct FAssetGLTFMalformedTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetGLTFMalformedTestResult RunAssetGLTFMalformedTests();
