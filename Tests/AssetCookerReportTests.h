#pragma once

struct FAssetCookerReportTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerReportTestResult RunAssetCookerReportTests();
