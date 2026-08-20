#pragma once

struct FMetalDeviceTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalDeviceTestResult RunMetalDeviceTests();
