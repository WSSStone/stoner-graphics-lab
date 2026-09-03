#pragma once

struct FMetalOutputTransformNativeTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalOutputTransformNativeTestResult
RunMetalOutputTransformNativeTests();
