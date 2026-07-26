#pragma once

struct FLoggingAssertionTestResult
{
    int Passed = 0;
    int Failed = 0;
};

inline constexpr const char* GLoggingFatalChildArgument =
    "--stoner-test-logging-fatal-child";
inline constexpr const char* GLoggingAssertionChildArgument =
    "--stoner-test-logging-assertion-child";

[[nodiscard]] FLoggingAssertionTestResult RunLoggingAssertionTests(
    const char* TestExecutablePath);
