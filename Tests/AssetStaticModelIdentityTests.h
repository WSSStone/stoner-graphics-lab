#pragma once

struct FAssetStaticModelIdentityTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetStaticModelIdentityTestResult
RunAssetStaticModelIdentityTests();
