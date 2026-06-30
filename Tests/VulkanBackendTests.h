#pragma once

struct FVulkanBackendTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FVulkanBackendTestResult RunVulkanBackendTests();
