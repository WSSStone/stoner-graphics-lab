#pragma once

struct FMetalShaderRuntimeTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalShaderRuntimeTestResult RunMetalShaderRuntimeTests();
