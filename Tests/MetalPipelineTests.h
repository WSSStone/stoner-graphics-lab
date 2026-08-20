#pragma once

struct FMetalPipelineTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalPipelineTestResult RunMetalPipelineTests();
