#pragma once

struct FMetalShaderCookerTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalShaderCookerTestResult RunMetalShaderCookerTests();
