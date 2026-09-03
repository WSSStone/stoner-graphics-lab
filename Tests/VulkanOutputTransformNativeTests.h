#pragma once

struct FVulkanOutputTransformNativeTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FVulkanOutputTransformNativeTestResult
RunVulkanOutputTransformNativeTests();
