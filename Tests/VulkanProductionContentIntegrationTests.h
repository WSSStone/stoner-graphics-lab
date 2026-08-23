#pragma once

struct FVulkanProductionContentIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FVulkanProductionContentIntegrationTestResult
RunVulkanProductionContentIntegrationTests();
