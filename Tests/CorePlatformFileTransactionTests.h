#pragma once

struct FCorePlatformFileTransactionTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FCorePlatformFileTransactionTestResult
RunCorePlatformFileTransactionTests();
