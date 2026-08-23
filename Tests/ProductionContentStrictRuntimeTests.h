#pragma once

struct FProductionContentStrictRuntimeTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FProductionContentStrictRuntimeTestResult
RunProductionContentStrictRuntimeTests();
