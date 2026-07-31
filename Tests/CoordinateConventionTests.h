#pragma once

struct FCoordinateConventionTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCoordinateConventionTestResult RunCoordinateConventionTests();
