#pragma once

#include "MetalTestSupport.h"

struct FMetalLifecycleStressTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalLifecycleStressTestResult RunMetalLifecycleStressTests(
    const FMetalTestOptions& Options = {});
