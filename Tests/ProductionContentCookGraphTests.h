#pragma once

struct FProductionContentCookGraphTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FProductionContentCookGraphTestResult
RunProductionContentCookGraphTests();
