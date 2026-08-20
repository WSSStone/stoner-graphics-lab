#pragma once

struct FMetalShaderCompilerTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalShaderCompilerTestResult RunMetalShaderCompilerTests();
