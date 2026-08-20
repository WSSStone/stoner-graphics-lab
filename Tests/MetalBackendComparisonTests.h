#pragma once

struct FMetalBackendComparisonTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FMetalBackendComparisonTestResult RunMetalBackendComparisonTests();
