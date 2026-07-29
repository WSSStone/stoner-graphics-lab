#include "AssetTests.h"

#include "AssetCoreTests.h"
#include "AssetImageTextureTests.h"

FAssetTestResult RunAssetTests()
{
    const FAssetCoreTestResult Core = RunAssetCoreTests();
    const FAssetImageTextureTestResult Image = RunAssetImageTextureTests();
    return {
        Core.Passed + Image.Passed,
        Core.Failed + Image.Failed};
}
