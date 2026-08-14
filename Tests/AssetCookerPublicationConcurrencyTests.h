#pragma once

struct FAssetCookerPublicationConcurrencyTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FAssetCookerPublicationConcurrencyTestResult
RunAssetCookerPublicationConcurrencyTests(const char* ProbeExecutable);
