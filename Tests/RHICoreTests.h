#pragma once

struct FRHICoreTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRHICoreTestResult RunRHICoreTests();
