#pragma once

struct FRendererForwardPipelineTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererForwardPipelineTestResult RunRendererForwardPipelineTests();
