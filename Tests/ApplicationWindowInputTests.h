#pragma once

struct FApplicationWindowInputTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FApplicationWindowInputTestResult RunApplicationWindowInputTests();
