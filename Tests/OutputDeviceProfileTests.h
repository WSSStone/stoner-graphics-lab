#pragma once

struct FOutputDeviceProfileTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FOutputDeviceProfileTestResult RunOutputDeviceProfileTests();
