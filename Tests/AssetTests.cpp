#include "AssetTests.h"

#include "AssetCoreTests.h"
#include "AssetImageTextureTests.h"
#include "AssetKTX2Tests.h"

FAssetTestResult RunAssetTests(
    const FAssetKTX2TestOptions& Options,
    const FAssetMaterialShaderTestOptions& MaterialShaderOptions)
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
    return {
        Core.Passed + Image.Passed + KTX2.Passed + MaterialShader.Passed +
            StaticMesh.Passed + GLTFPolicy.Passed + GLTFContainer.Passed,
        Core.Failed + Image.Failed + KTX2.Failed + MaterialShader.Failed +
            StaticMesh.Failed + GLTFPolicy.Failed + GLTFContainer.Failed};
}
