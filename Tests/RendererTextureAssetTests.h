#pragma once

struct FRendererTextureAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererTextureAssetTestResult
RunRendererTextureAssetTests();
