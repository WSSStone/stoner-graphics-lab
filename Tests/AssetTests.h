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
#include "AssetStaticModelDeterminism.h"
#include "AssetStaticModelConcurrency.h"
#include "AssetStaticModelBenchmark.h"

#include <string>

struct FAssetStaticModelTestOptions
{
    int DeterminismRuns = 20;
    int PerformanceRuns = 0;
    double PerformanceMaxSeconds = 5.0;
    std::string PerformanceFixture;
};

struct FAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options = {},
    const FAssetMaterialShaderTestOptions& MaterialShaderOptions = {},
    const FAssetStaticModelTestOptions& StaticModelOptions = {});
