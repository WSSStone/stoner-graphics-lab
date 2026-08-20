#pragma once

struct FMetalShaderDerivationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalShaderDerivationTestResult
RunMetalShaderDerivationTests();
