#pragma once

struct FLoggingAssertionTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FLoggingAssertionTestResult RunLoggingAssertionTests();
