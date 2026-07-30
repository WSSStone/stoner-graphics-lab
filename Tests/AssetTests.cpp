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
    return {
        Core.Passed + Image.Passed + KTX2.Passed + MaterialShader.Passed,
        Core.Failed + Image.Failed + KTX2.Failed + MaterialShader.Failed};
}
