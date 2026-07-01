#pragma once

struct FRendererRenderGraphTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererRenderGraphTestResult RunRendererRenderGraphTests();
