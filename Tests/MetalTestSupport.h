#pragma once

#include <cstdint>
#include <string>

struct FMetalTestOptions
{
    std::string ReportPath;
    std::uint32_t DeterminismRuns = 20;
    std::uint32_t LifecycleIterations = 10000;
    bool bRequestNative = false;
    bool bRequestVisible = false;
};
