#pragma once

struct FRendererStaticMeshFailureTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererStaticMeshFailureTestResult
RunRendererStaticMeshFailureTests();
