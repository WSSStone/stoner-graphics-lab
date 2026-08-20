#pragma once

#include "MetalTestSupport.h"

struct FMetalTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalTestResult RunMetalTests(const FMetalTestOptions& Options = {});
