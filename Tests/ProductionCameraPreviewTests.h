#pragma once

struct FProductionCameraPreviewTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FProductionCameraPreviewTestResult
RunProductionCameraPreviewTests();
