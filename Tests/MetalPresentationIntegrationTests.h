#pragma once

struct FMetalPresentationIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FMetalPresentationIntegrationTestResult
RunMetalPresentationIntegrationTests(bool bRequireVisible);
