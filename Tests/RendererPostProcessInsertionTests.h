#pragma once

struct FRendererPostProcessInsertionTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererPostProcessInsertionTestResult
RunRendererPostProcessInsertionTests();
