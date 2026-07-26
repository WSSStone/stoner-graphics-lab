// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "ApplicationSceneEcsTests.h"
#include "ApplicationWindowInputTests.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"
#include "LoggingAssertionTests.h"
#include "CorePlatformTests.h"
#include "DeferredRenderingTests.h"
#include "DeferredNativeIntegrationTests.h"
#include "RHICoreTests.h"
#include "RendererForwardPipelineTests.h"
#include "RendererComparisonTests.h"
#include "RendererMaterialShaderTests.h"
#include "RendererRenderGraphTests.h"
#include "VulkanBackendTests.h"
#include "VulkanNativeIntegrationTests.h"
#include "TriangleDemoIntegrationTests.h"

#include <cstring>

int main(int ArgCount, char* Arguments[])
{
    if (ArgCount == 2 &&
        std::strcmp(Arguments[1], GLoggingFatalChildArgument) == 0)
    {
        SG_LOG(Stoner::Core::LogCore, Fatal, "isolated fatal logging probe");
        return 42;
    }
    if (ArgCount == 2 &&
        std::strcmp(Arguments[1], GLoggingAssertionChildArgument) == 0)
    {
        SG_CHECK(false);
        return 42;
    }

    const FCoreFoundationTestResult CoreResult = RunCoreFoundationTests();
    const FCoreMathTestResult MathResult = RunCoreMathTests();
    const FLoggingAssertionTestResult LogResult =
        RunLoggingAssertionTests(Arguments[0]);
    const FCorePlatformTestResult PlatformResult = RunCorePlatformTests();
    const FApplicationWindowInputTestResult ApplicationResult = RunApplicationWindowInputTests();
    const FApplicationSceneEcsTestResult SceneResult = RunApplicationSceneEcsTests();
    const FRHICoreTestResult RHIResult = RunRHICoreTests();
    const FDeferredRenderingTestResult DeferredResult = RunDeferredRenderingTests();
    const FDeferredNativeIntegrationTestResult DeferredNativeResult =
        RunDeferredNativeIntegrationTests();
    const FRendererForwardPipelineTestResult ForwardPipelineResult = RunRendererForwardPipelineTests();
    const FRendererComparisonTestResult ComparisonResult = RunRendererComparisonTests();
    const FRendererMaterialShaderTestResult MaterialShaderResult = RunRendererMaterialShaderTests();
    const FRendererRenderGraphTestResult RenderGraphResult = RunRendererRenderGraphTests();
    const FVulkanBackendTestResult VulkanResult = RunVulkanBackendTests();
    const FVulkanNativeIntegrationTestResult NativeVulkanResult = RunVulkanNativeIntegrationTests();
    const FTriangleDemoIntegrationTestResult DemoResult = RunTriangleDemoIntegrationTests();
    return CoreResult.Failed == 0 && MathResult.Failed == 0 &&
        LogResult.Failed == 0 && PlatformResult.Failed == 0 &&
        ApplicationResult.Failed == 0 &&
        SceneResult.Failed == 0 &&
        RHIResult.Failed == 0 && DeferredResult.Failed == 0 &&
        DeferredNativeResult.Failed == 0 &&
        ForwardPipelineResult.Failed == 0 && ComparisonResult.Failed == 0 &&
        MaterialShaderResult.Failed == 0 && RenderGraphResult.Failed == 0 &&
        VulkanResult.Failed == 0 && NativeVulkanResult.Failed == 0 && DemoResult.Failed == 0 ? 0 : 1;
}
