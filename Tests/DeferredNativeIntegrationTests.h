#pragma once

struct FDeferredNativeIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FDeferredNativeIntegrationTestResult RunDeferredNativeIntegrationTests();
