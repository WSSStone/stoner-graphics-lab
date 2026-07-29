#pragma once

struct FAssetImageTextureTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetImageTextureTestResult RunAssetImageTextureTests();
