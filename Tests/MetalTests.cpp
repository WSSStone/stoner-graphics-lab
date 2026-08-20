#include "MetalTests.h"

#include <iostream>

FMetalTestResult RunMetalTests(const FMetalTestOptions& Options)
{
    FMetalTestResult Result;
    std::cout << "[INFO] Metal test scaffold: native="
              << (Options.bRequestNative ? "requested" : "not-requested")
              << ", visible="
              << (Options.bRequestVisible ? "requested" : "not-requested")
              << ", determinism-runs=" << Options.DeterminismRuns
              << ", lifecycle-iterations=" << Options.LifecycleIterations
              << '\n';
    return Result;
}
