#pragma once

struct FAssetGLTFImageDependencyTestResult { int Passed = 0; int Failed = 0; };
[[nodiscard]] FAssetGLTFImageDependencyTestResult
RunAssetGLTFImageDependencyTests();
