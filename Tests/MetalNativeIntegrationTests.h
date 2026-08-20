#pragma once

struct FMetalNativeIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalNativeIntegrationTestResult
RunMetalNativeIntegrationTests(bool bRequireNative = false);
