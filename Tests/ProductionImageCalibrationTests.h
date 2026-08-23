#pragma once

struct FProductionImageCalibrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FProductionImageCalibrationTestResult
RunProductionImageCalibrationTests();
