#pragma once

struct FRendererStaticModelRealizationFailureTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererStaticModelRealizationFailureTestResult
RunRendererStaticModelRealizationFailureTests();
