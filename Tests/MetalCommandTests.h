#pragma once

struct FMetalCommandTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalCommandTestResult RunMetalCommandTests();
