#pragma once

struct FAssetCookerWorkflowTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerWorkflowTestResult RunAssetCookerWorkflowTests();
