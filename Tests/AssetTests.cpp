#include "AssetTests.h"

#include "AssetCoreTests.h"
#include "AssetImageTextureTests.h"
#include "AssetKTX2Tests.h"

FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options,
    const FAssetMaterialShaderTestOptions& MaterialShaderOptions,
    const FAssetStaticModelTestOptions& StaticModelOptions,
    const FAssetManagerTestOptions& AssetManagerOptions)
{
    const FAssetCoreTestResult Core = RunAssetCoreTests();
    const FAssetImageTextureTestResult Image = RunAssetImageTextureTests();
    const FAssetKTX2TestResult KTX2 = RunAssetKTX2Tests(Options);
    const FAssetMaterialShaderTestResult MaterialShader =
        RunAssetMaterialShaderTests(MaterialShaderOptions);
    const FAssetStaticMeshGeometryTestResult StaticMesh =
        RunAssetStaticMeshGeometryTests();
    const FAssetGLTFPolicyTestResult GLTFPolicy = RunAssetGLTFPolicyTests();
    const FAssetGLTFContainerTestResult GLTFContainer =
        RunAssetGLTFContainerTests();
    const FAssetStaticModelHierarchyTestResult StaticModelHierarchy =
        RunAssetStaticModelHierarchyTests();
    const FAssetStaticModelIdentityTestResult StaticModelIdentity =
        RunAssetStaticModelIdentityTests();
    const auto StaticModelDeterminism =
        RunAssetStaticModelDeterminismTests(StaticModelOptions);
    const auto StaticModelConcurrency = RunAssetStaticModelConcurrencyTests();
    const auto StaticModelBenchmark =
        RunAssetStaticModelBenchmark(StaticModelOptions);
    const auto CookerProfile = RunAssetCookerProfileTests();
    const auto DerivedKey = RunAssetCookerDerivedKeyTests();
    const auto Equivalence = RunAssetCookerEquivalenceTests();
    const auto Manifest = RunAssetCookerManifestTests();
    const auto PayloadCodec = RunAssetCookerPayloadCodecTests();
    const auto AssetManager = RunAssetManagerTests(AssetManagerOptions);
    const FAssetGLTFMaterialTestResult GLTFMaterial =
        RunAssetGLTFMaterialTests();
    const FAssetGLTFImageDependencyTestResult GLTFImage =
        RunAssetGLTFImageDependencyTests();
    const FAssetGLTFMalformedTestResult GLTFMalformed =
        RunAssetGLTFMalformedTests();
    const FAssetGLTFResolverTestResult GLTFResolver =
        RunAssetGLTFResolverTests();
    const FAssetGLTFLimitTestResult GLTFLimit = RunAssetGLTFLimitTests();
    const FAssetGLTFDiagnosticTestResult GLTFDiagnostic =
        RunAssetGLTFDiagnosticTests();
    return {
        Core.Passed + Image.Passed + KTX2.Passed + MaterialShader.Passed +
            StaticMesh.Passed + GLTFPolicy.Passed + GLTFContainer.Passed +
            StaticModelHierarchy.Passed + StaticModelIdentity.Passed +
            StaticModelDeterminism.Passed + StaticModelConcurrency.Passed +
            StaticModelBenchmark.Passed +
            CookerProfile.Passed + DerivedKey.Passed + Equivalence.Passed +
            Manifest.Passed +
            PayloadCodec.Passed +
            AssetManager.Passed +
            GLTFMaterial.Passed + GLTFImage.Passed + GLTFMalformed.Passed +
            GLTFResolver.Passed + GLTFLimit.Passed + GLTFDiagnostic.Passed,
        Core.Failed + Image.Failed + KTX2.Failed + MaterialShader.Failed +
            StaticMesh.Failed + GLTFPolicy.Failed + GLTFContainer.Failed +
            StaticModelHierarchy.Failed + StaticModelIdentity.Failed +
            StaticModelDeterminism.Failed + StaticModelConcurrency.Failed +
            StaticModelBenchmark.Failed +
            CookerProfile.Failed + DerivedKey.Failed + Equivalence.Failed +
            Manifest.Failed +
            PayloadCodec.Failed +
            AssetManager.Failed +
            GLTFMaterial.Failed + GLTFImage.Failed + GLTFMalformed.Failed +
            GLTFResolver.Failed + GLTFLimit.Failed + GLTFDiagnostic.Failed};
}

FAssetManagerKernelTestResult RunAssetManagerTests(
    const FAssetManagerTestOptions& Options)
{
    (void)Options;
    const auto Kernel = RunAssetManagerKernelTests();
    const auto Contract = RunAssetManagerContractTests();
    const auto Development = RunAssetManagerDevelopmentTests();
    const auto Dependency = RunAssetManagerDependencyTests();
    const auto ManagerEquivalence = RunAssetManagerEquivalenceTests();
    const auto Coalescing = RunAssetManagerCoalescingTests();
    const auto Cancellation = RunAssetManagerCancellationTests();
    const auto Cache = RunAssetManagerCacheTests();
    const auto Lifetime = RunAssetManagerLifetimeTests();
    const auto Shutdown = RunAssetManagerShutdownTests();
    const auto GenerationLease = RunAssetManagerGenerationLeaseTests();
    const auto GenerationLeaseProcess =
        RunAssetManagerGenerationLeaseProcessTests(
            Options.GenerationLeaseProbe.c_str());
    const auto Completion = RunAssetManagerCompletionTests();
    const auto Inspection = RunAssetManagerInspectionTests();
    const auto Stress = RunAssetManagerStressTests();
    const auto Benchmark = RunAssetManagerBenchmark(
        Options.BenchmarkEnabled, Options.BenchmarkCiProfile,
        Options.BenchmarkReport);
    return {
        Kernel.Passed + Contract.Passed + Development.Passed +
            Dependency.Passed + ManagerEquivalence.Passed +
            Coalescing.Passed + Cancellation.Passed + Cache.Passed +
            Lifetime.Passed + Shutdown.Passed + GenerationLease.Passed +
            GenerationLeaseProcess.Passed + Completion.Passed +
            Inspection.Passed + Stress.Passed + Benchmark.Passed,
        Kernel.Failed + Contract.Failed + Development.Failed +
            Dependency.Failed + ManagerEquivalence.Failed +
            Coalescing.Failed + Cancellation.Failed + Cache.Failed +
            Lifetime.Failed + Shutdown.Failed + GenerationLease.Failed +
            GenerationLeaseProcess.Failed + Completion.Failed +
            Inspection.Failed + Stress.Failed + Benchmark.Failed};
}
