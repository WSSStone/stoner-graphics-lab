#pragma once

struct FAssetManagerCoalescingTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetManagerCoalescingTestResult
RunAssetManagerCoalescingTests();

