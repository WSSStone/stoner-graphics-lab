#pragma once

struct FMetalProductionContentIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalProductionContentIntegrationTestResult
RunMetalProductionContentIntegrationTests();
