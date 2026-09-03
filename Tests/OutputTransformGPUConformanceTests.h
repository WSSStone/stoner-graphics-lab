#pragma once

struct FOutputTransformGPUConformanceTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FOutputTransformGPUConformanceTestResult
RunOutputTransformGPUConformanceTests();
