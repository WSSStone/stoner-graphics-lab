#pragma once

struct FAssetStaticModelHierarchyTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetStaticModelHierarchyTestResult
RunAssetStaticModelHierarchyTests();
