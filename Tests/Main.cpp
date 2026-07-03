// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "ApplicationWindowInputTests.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"
#include "LoggingAssertionTests.h"
#include "CorePlatformTests.h"
#include "RHICoreTests.h"
#include "RendererForwardPipelineTests.h"
#include "RendererMaterialShaderTests.h"
#include "RendererRenderGraphTests.h"
#include "VulkanBackendTests.h"

int main()
{
    const FCoreFoundationTestResult CoreResult = RunCoreFoundationTests();
    const FCoreMathTestResult MathResult = RunCoreMathTests();
    const FLoggingAssertionTestResult LogResult = RunLoggingAssertionTests();
    const FCorePlatformTestResult PlatformResult = RunCorePlatformTests();
    const FApplicationWindowInputTestResult ApplicationResult = RunApplicationWindowInputTests();
    const FRHICoreTestResult RHIResult = RunRHICoreTests();
    const FRendererForwardPipelineTestResult ForwardPipelineResult = RunRendererForwardPipelineTests();
    const FRendererMaterialShaderTestResult MaterialShaderResult = RunRendererMaterialShaderTests();
    const FRendererRenderGraphTestResult RenderGraphResult = RunRendererRenderGraphTests();
    const FVulkanBackendTestResult VulkanResult = RunVulkanBackendTests();
    return CoreResult.Failed == 0 && MathResult.Failed == 0 &&
        LogResult.Failed == 0 && PlatformResult.Failed == 0 &&
        ApplicationResult.Failed == 0 &&
        RHIResult.Failed == 0 && ForwardPipelineResult.Failed == 0 &&
        MaterialShaderResult.Failed == 0 && RenderGraphResult.Failed == 0 &&
        VulkanResult.Failed == 0 ? 0 : 1;
}
