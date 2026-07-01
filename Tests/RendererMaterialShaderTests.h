#pragma once

struct FRendererMaterialShaderTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererMaterialShaderTestResult RunRendererMaterialShaderTests();
