#pragma once

#include "AssetKTX2Tests.h"
#include "AssetGLTFContainerTests.h"
#include "AssetGLTFMaterialTests.h"
#include "AssetGLTFImageDependencyTests.h"
#include "AssetGLTFMalformedTests.h"
#include "AssetGLTFResolverTests.h"
#include "AssetGLTFLimitTests.h"
#include "AssetGLTFDiagnosticTests.h"
#include "AssetGLTFPolicyTests.h"
#include "AssetMaterialShaderTests.h"
#include "AssetStaticMeshGeometryTests.h"
#include "AssetStaticModelHierarchyTests.h"
#include "AssetStaticModelIdentityTests.h"

struct FAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options = {},
    const FAssetMaterialShaderTestOptions& MaterialShaderOptions = {});
