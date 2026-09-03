#pragma once

struct FRendererOutputTransformTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererOutputTransformTestResult
RunRendererOutputTransformTests();
