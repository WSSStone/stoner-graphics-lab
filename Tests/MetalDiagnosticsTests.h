#pragma once

struct FMetalDiagnosticsTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalDiagnosticsTestResult RunMetalDiagnosticsTests();
