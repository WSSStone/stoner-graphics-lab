#pragma once

struct FOutputPresentationLifecycleTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FOutputPresentationLifecycleTestResult
RunOutputPresentationLifecycleTests();
