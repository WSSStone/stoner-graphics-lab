#pragma once

struct FAssetGLTFMaterialTestResult { int Passed = 0; int Failed = 0; };
[[nodiscard]] FAssetGLTFMaterialTestResult RunAssetGLTFMaterialTests();
