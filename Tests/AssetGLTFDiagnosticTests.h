#pragma once

struct FAssetGLTFDiagnosticTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetGLTFDiagnosticTestResult RunAssetGLTFDiagnosticTests();
