#pragma once

struct FProductionContentTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FProductionContentTestResult RunProductionContentTests();
