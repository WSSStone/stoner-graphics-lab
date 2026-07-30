#pragma once

struct FRendererMaterialShaderAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererMaterialShaderAssetTestResult
RunRendererMaterialShaderAssetTests();
