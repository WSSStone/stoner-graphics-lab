// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "ApplicationSceneEcsTests.h"
#include "ApplicationWindowInputTests.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"
#include "CorePlatformOwnershipTests.h"
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
#include "AssetCoreTests.h"
#include "TestSuiteRegistry.h"
#include "TestSuiteRegistryTests.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

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

    FTestSuiteRegistry Registry;
    Registry.Register("application-scene", [] { return RunApplicationSceneEcsTests().Failed == 0 ? 0 : 1; });
    Registry.Register("application-window", [] { return RunApplicationWindowInputTests().Failed == 0 ? 0 : 1; });
    Registry.Register("asset", [] { return RunAssetCoreTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-foundation", [] { return RunCoreFoundationTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-math", [] { return RunCoreMathTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-platform", [] { return RunCorePlatformTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-platform-ownership", [] { return RunCorePlatformOwnershipTests().Failed == 0 ? 0 : 1; });
    Registry.Register("deferred-native", [] { return RunDeferredNativeIntegrationTests().Failed == 0 ? 0 : 1; });
    Registry.Register("deferred-renderer", [] { return RunDeferredRenderingTests().Failed == 0 ? 0 : 1; });
    Registry.Register("logging", [Executable = std::string(Arguments[0])] {
        return RunLoggingAssertionTests(Executable.c_str()).Failed == 0 ? 0 : 1;
    });
    Registry.Register("renderer-comparison", [] { return RunRendererComparisonTests().Failed == 0 ? 0 : 1; });
    Registry.Register("renderer-forward", [] { return RunRendererForwardPipelineTests().Failed == 0 ? 0 : 1; });
    Registry.Register("renderer-material", [] { return RunRendererMaterialShaderTests().Failed == 0 ? 0 : 1; });
    Registry.Register("renderer-render-graph", [] { return RunRendererRenderGraphTests().Failed == 0 ? 0 : 1; });
    Registry.Register("rhi", [] { return RunRHICoreTests().Failed == 0 ? 0 : 1; });
    Registry.Register("test-runner", [Executable = std::string(Arguments[0])] {
        return RunTestSuiteRegistryTests(Executable.c_str()).Failed == 0 ? 0 : 1;
    });
    Registry.Register("triangle-demo", [] { return RunTriangleDemoIntegrationTests().Failed == 0 ? 0 : 1; });
    Registry.Register("vulkan", [] { return RunVulkanBackendTests().Failed == 0 ? 0 : 1; });
    Registry.Register("vulkan-native", [] { return RunVulkanNativeIntegrationTests().Failed == 0 ? 0 : 1; });

    std::vector<std::string> ParsedArguments;
    for (int Index = 1; Index < ArgCount; ++Index)
    {
        ParsedArguments.emplace_back(Arguments[Index]);
    }
    return Registry.Execute(ParsedArguments, std::cout, std::cerr);
}
