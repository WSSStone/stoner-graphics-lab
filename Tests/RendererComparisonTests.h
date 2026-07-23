#pragma once

struct FRendererComparisonTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererComparisonTestResult RunRendererComparisonTests();
