#pragma once

struct FRendererPostProcessGraphTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererPostProcessGraphTestResult
RunRendererPostProcessGraphTests();
