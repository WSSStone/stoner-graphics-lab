#include "AssetTests.h"

#include "AssetCoreTests.h"
#include "AssetImageTextureTests.h"
#include "AssetKTX2Tests.h"

FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options,
    const FAssetMaterialShaderTestOptions& MaterialShaderOptions,
    const FAssetStaticModelTestOptions& StaticModelOptions)
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
            GLTFMaterial.Passed + GLTFImage.Passed + GLTFMalformed.Passed +
            GLTFResolver.Passed + GLTFLimit.Passed + GLTFDiagnostic.Passed,
        Core.Failed + Image.Failed + KTX2.Failed + MaterialShader.Failed +
            StaticMesh.Failed + GLTFPolicy.Failed + GLTFContainer.Failed +
            StaticModelHierarchy.Failed + StaticModelIdentity.Failed +
            StaticModelDeterminism.Failed + StaticModelConcurrency.Failed +
            StaticModelBenchmark.Failed +
            GLTFMaterial.Failed + GLTFImage.Failed + GLTFMalformed.Failed +
            GLTFResolver.Failed + GLTFLimit.Failed + GLTFDiagnostic.Failed};
}
