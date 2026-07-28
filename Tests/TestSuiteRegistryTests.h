#pragma once

struct FTestSuiteRegistryTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FTestSuiteRegistryTestResult RunTestSuiteRegistryTests(
    const char* ExecutablePath);
