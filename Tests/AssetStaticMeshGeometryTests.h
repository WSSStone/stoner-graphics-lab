#pragma once

struct FAssetStaticMeshGeometryTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetStaticMeshGeometryTestResult RunAssetStaticMeshGeometryTests();
