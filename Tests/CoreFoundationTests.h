#pragma once

struct FCoreFoundationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCoreFoundationTestResult RunCoreFoundationTests();
