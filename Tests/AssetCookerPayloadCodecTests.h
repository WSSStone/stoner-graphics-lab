#pragma once

struct FAssetCookerPayloadCodecTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetCookerPayloadCodecTestResult
RunAssetCookerPayloadCodecTests();
