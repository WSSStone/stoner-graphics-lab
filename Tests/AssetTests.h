#pragma once

#include "AssetKTX2Tests.h"
#include "AssetGLTFContainerTests.h"
#include "AssetGLTFPolicyTests.h"
#include "AssetMaterialShaderTests.h"
#include "AssetStaticMeshGeometryTests.h"

struct FAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options = {},
    const FAssetMaterialShaderTestOptions& MaterialShaderOptions = {});
