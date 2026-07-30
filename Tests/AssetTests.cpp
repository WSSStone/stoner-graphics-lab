#include "AssetTests.h"

#include "AssetCoreTests.h"
#include "AssetImageTextureTests.h"
#include "AssetKTX2Tests.h"

FAssetTestResult RunAssetTests(const FAssetKTX2TestOptions& Options)
{
    const FAssetCoreTestResult Core = RunAssetCoreTests();
    const FAssetImageTextureTestResult Image = RunAssetImageTextureTests();
    const FAssetKTX2TestResult KTX2 = RunAssetKTX2Tests(Options);
    return {
        Core.Passed + Image.Passed + KTX2.Passed,
        Core.Failed + Image.Failed + KTX2.Failed};
}
