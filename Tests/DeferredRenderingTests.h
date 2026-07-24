#pragma once

struct FDeferredRenderingTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FDeferredRenderingTestResult RunDeferredRenderingTests();
