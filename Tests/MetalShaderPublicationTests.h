#pragma once

struct FMetalShaderPublicationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalShaderPublicationTestResult
RunMetalShaderPublicationTests();
