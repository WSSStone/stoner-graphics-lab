#pragma once

struct FCoreMathTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCoreMathTestResult RunCoreMathTests();
