#pragma once

struct FRendererStaticMeshTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererStaticMeshTestResult RunRendererStaticMeshTests();
