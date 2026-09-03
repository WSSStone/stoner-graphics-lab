#pragma once

struct FRendererOutputTransformMathTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererOutputTransformMathTestResult
RunRendererOutputTransformMathTests();
