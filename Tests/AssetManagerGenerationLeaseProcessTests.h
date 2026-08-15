#pragma once

struct FAssetManagerGenerationLeaseProcessTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerGenerationLeaseProcessTestResult
RunAssetManagerGenerationLeaseProcessTests(const char* ProbeExecutable);
