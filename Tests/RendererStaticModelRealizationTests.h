#pragma once

struct FRendererStaticModelRealizationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererStaticModelRealizationTestResult
RunRendererStaticModelRealizationTests();
