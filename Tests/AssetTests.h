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
#include "AssetCookerProfileTests.h"
#include "AssetCookerDerivedKeyTests.h"
#include "AssetCookerEquivalenceTests.h"
#include "AssetCookerManifestTests.h"
#include "AssetCookerPayloadCodecTests.h"
#include "AssetManagerKernelTests.h"
#include "AssetManagerContractTests.h"
#include "AssetManagerDevelopmentTests.h"
#include "AssetManagerDependencyTests.h"
#include "AssetManagerEquivalenceTests.h"
#include "AssetManagerCoalescingTests.h"
#include "AssetManagerCancellationTests.h"
#include "AssetManagerCacheTests.h"
#include "AssetManagerLifetimeTests.h"
#include "AssetManagerShutdownTests.h"
#include "AssetManagerGenerationLeaseTests.h"
#include "AssetManagerGenerationLeaseProcessTests.h"
#include "AssetManagerCompletionTests.h"
#include "AssetManagerInspectionTests.h"
#include "AssetManagerStressTests.h"
#include "AssetManagerBenchmark.h"

#include <string>

struct FAssetStaticModelTestOptions
{
    int DeterminismRuns = 20;
    int PerformanceRuns = 0;
    double PerformanceMaxSeconds = 5.0;
    std::string PerformanceFixture;
};

struct FAssetManagerTestOptions
{
    bool BenchmarkEnabled = false;
    bool BenchmarkCiProfile = false;
    std::string BenchmarkReport;
    std::string GenerationLeaseProbe;
};

struct FAssetTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options = {},
    const FAssetMaterialShaderTestOptions& MaterialShaderOptions = {},
    const FAssetStaticModelTestOptions& StaticModelOptions = {},
    const FAssetManagerTestOptions& AssetManagerOptions = {});

[[nodiscard]] FAssetManagerKernelTestResult RunAssetManagerTests(
    const FAssetManagerTestOptions& Options = {});
