#pragma once

struct FAssetCookerInputSnapshotTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerInputSnapshotTestResult
RunAssetCookerInputSnapshotTests();
