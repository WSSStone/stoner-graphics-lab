// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "ApplicationSceneEcsTests.h"
#include "ApplicationWindowInputTests.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"
#include "CoordinateConventionTests.h"
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
#include "RendererKTX2TextureTests.h"
#include "RendererTextureAssetTests.h"
#include "VulkanBackendTests.h"
#include "VulkanNativeIntegrationTests.h"
#include "TriangleDemoIntegrationTests.h"
#include "AssetTests.h"
#include "AssetGLTFContainerTests.h"
#include "AssetGLTFPolicyTests.h"
#include "AssetMaterialShaderTests.h"
#include "AssetStaticMeshGeometryTests.h"
#include "AssetStaticModelHierarchyTests.h"
#include "AssetStaticModelIdentityTests.h"
#include "RendererMaterialShaderAssetTests.h"
#include "TestSuiteRegistry.h"
#include "TestSuiteRegistryTests.h"

#include <cstring>
#include <charconv>
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

    FAssetKTX2TestOptions KTX2Options;
    FAssetMaterialShaderTestOptions MaterialShaderOptions;
    std::vector<std::string> ParsedArguments;
    for (int Index = 1; Index < ArgCount; ++Index)
    {
        const std::string Argument = Arguments[Index];
        if (Argument == "--emit-ktx2-dir" ||
            Argument == "--emit-ktx2-source-dir" ||
            Argument == "--report" ||
            Argument == "--ktx2-determinism-runs" ||
            Argument == "--material-shader-report" ||
            Argument == "--material-shader-determinism-runs")
        {
            if (Index + 1 >= ArgCount)
            {
                std::cerr << "Missing value after " << Argument << '\n';
                return 2;
            }
            const std::string Value = Arguments[++Index];
            if (Argument == "--material-shader-report")
            {
                MaterialShaderOptions.ReportPath = Value.c_str();
            }
            else if (Argument == "--emit-ktx2-dir")
            {
                KTX2Options.EmitDirectory = Value;
            }
            else if (Argument == "--emit-ktx2-source-dir")
            {
                KTX2Options.EmitSourceDirectory = Value;
            }
            else if (Argument == "--report")
            {
                KTX2Options.ReportPath = Value;
            }
            else if (Argument == "--ktx2-determinism-runs")
            {
                int Runs = 0;
                const auto Parsed = std::from_chars(
                    Value.data(), Value.data() + Value.size(), Runs);
                if (Parsed.ec != std::errc{} ||
                    Parsed.ptr != Value.data() + Value.size() ||
                    Runs <= 0 || Runs > 1000)
                {
                    std::cerr
                        << "Invalid --ktx2-determinism-runs value\n";
                    return 2;
                }
                KTX2Options.DeterminismRuns = Runs;
            }
            else
            {
                int Runs = 0;
                const auto Parsed = std::from_chars(
                    Value.data(), Value.data() + Value.size(), Runs);
                if (Parsed.ec != std::errc{} ||
                    Parsed.ptr != Value.data() + Value.size() ||
                    Runs <= 0 || Runs > 1000)
                {
                    std::cerr
                        << "Invalid --material-shader-determinism-runs value\n";
                    return 2;
                }
                MaterialShaderOptions.DeterminismRuns = Runs;
            }
            continue;
        }
        ParsedArguments.push_back(Argument);
    }

    FTestSuiteRegistry Registry;
    Registry.Register("application-scene", [] { return RunApplicationSceneEcsTests().Failed == 0 ? 0 : 1; });
    Registry.Register("application-window", [] { return RunApplicationWindowInputTests().Failed == 0 ? 0 : 1; });
    Registry.Register("asset", [KTX2Options, MaterialShaderOptions] {
        return RunAssetTests(KTX2Options, MaterialShaderOptions).Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-material-shader", [MaterialShaderOptions] {
        return RunAssetMaterialShaderTests(MaterialShaderOptions).Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-static-mesh", [] {
        const auto Geometry = RunAssetStaticMeshGeometryTests();
        const auto Policy = RunAssetGLTFPolicyTests();
        const auto Container = RunAssetGLTFContainerTests();
        return Geometry.Failed == 0 && Policy.Failed == 0 &&
            Container.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-static-model", [] {
        const auto Hierarchy = RunAssetStaticModelHierarchyTests();
        const auto Identity = RunAssetStaticModelIdentityTests();
        return Hierarchy.Failed == 0 && Identity.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-ktx2-encoder", [] {
        return RunAssetKTX2EncoderTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("core-foundation", [] { return RunCoreFoundationTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-math", [] { return RunCoreMathTests().Failed == 0 ? 0 : 1; });
    Registry.Register("coordinate-convention", [] { return RunCoordinateConventionTests().Failed == 0 ? 0 : 1; });
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
    Registry.Register("renderer-material-asset", [] {
        return RunRendererMaterialShaderAssetTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("renderer-render-graph", [] { return RunRendererRenderGraphTests().Failed == 0 ? 0 : 1; });
    Registry.Register("renderer-texture", [] {
        const auto AssetResult = RunRendererTextureAssetTests();
        const auto KTX2Result = RunRendererKTX2TextureTests();
        return AssetResult.Failed == 0 && KTX2Result.Failed == 0
            ? 0
            : 1;
    });
    Registry.Register("rhi", [] { return RunRHICoreTests().Failed == 0 ? 0 : 1; });
    Registry.Register("test-runner", [Executable = std::string(Arguments[0])] {
        return RunTestSuiteRegistryTests(Executable.c_str()).Failed == 0 ? 0 : 1;
    });
    Registry.Register("triangle-demo", [] { return RunTriangleDemoIntegrationTests().Failed == 0 ? 0 : 1; });
    Registry.Register("vulkan", [] { return RunVulkanBackendTests().Failed == 0 ? 0 : 1; });
    Registry.Register("vulkan-native", [] { return RunVulkanNativeIntegrationTests().Failed == 0 ? 0 : 1; });

    return Registry.Execute(ParsedArguments, std::cout, std::cerr);
}
