#pragma once

struct FMetalFailureInjectionTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalFailureInjectionTestResult
RunMetalFailureInjectionTests();
