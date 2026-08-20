#pragma once

#include "MetalTestSupport.h"

struct FMetalShaderRuntimeTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalShaderRuntimeTestResult RunMetalShaderRuntimeTests(
    const FMetalTestOptions& Options = {});
