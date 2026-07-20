#pragma once

struct FVulkanNativeIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FVulkanNativeIntegrationTestResult RunVulkanNativeIntegrationTests();
