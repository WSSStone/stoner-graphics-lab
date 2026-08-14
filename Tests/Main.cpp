// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "ApplicationSceneEcsTests.h"
#include "ApplicationWindowInputTests.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"
#include "CoordinateConventionTests.h"
#include "CorePlatformOwnershipTests.h"
#include "CorePlatformFileLeaseTests.h"
#include "CorePlatformFileTransactionTests.h"
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
#include "RendererStaticMeshTests.h"
#include "RendererStaticMeshFailureTests.h"
#include "VulkanBackendTests.h"
#include "VulkanNativeIntegrationTests.h"
#include "TriangleDemoIntegrationTests.h"
#include "AssetTests.h"
#include "AssetGLTFContainerTests.h"
#include "AssetGLTFMaterialTests.h"
#include "AssetGLTFImageDependencyTests.h"
#include "AssetGLTFMalformedTests.h"
#include "AssetGLTFResolverTests.h"
#include "AssetGLTFLimitTests.h"
#include "AssetGLTFDiagnosticTests.h"
#include "AssetGLTFPolicyTests.h"
#include "AssetMaterialShaderTests.h"
#include "AssetCookerGraphTests.h"
#include "AssetCookerSchedulerTests.h"
#include "AssetCookerInputSnapshotTests.h"
#include "AssetCookerSourceCatalogTests.h"
#include "AssetCookerDeterminismTests.h"
#include "AssetCookerDerivedDataTests.h"
#include "AssetCookerIncrementalTests.h"
#include "AssetCookerConcurrencyTests.h"
#include "AssetCookerPublicationTests.h"
#include "AssetCookerPublishedValidationTests.h"
#include "AssetCookerPublicationConcurrencyTests.h"
#include "AssetCookerTargetProfileTests.h"
#include "AssetCookerProfileInvalidationTests.h"
#include "AssetCookerCliTests.h"
#include "AssetCookerReportTests.h"
#include "AssetCookerWorkflowTests.h"
#include "AssetCookerBenchmark.h"
#include "AssetStaticMeshGeometryTests.h"
#include "AssetStaticModelHierarchyTests.h"
#include "AssetStaticModelIdentityTests.h"
#include "RendererMaterialShaderAssetTests.h"
#include "TestSuiteRegistry.h"
#include "TestSuiteRegistryTests.h"

#include <cstring>
#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
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
    FAssetStaticModelTestOptions StaticModelOptions;
    FAssetCookerBenchmarkOptions AssetCookerBenchmarkOptions;
    std::vector<std::string> ParsedArguments;
    std::filesystem::path LeaseProbePath =
        std::filesystem::path(Arguments[0]).parent_path() /
        "PlatformFileLeaseProbe";
    std::filesystem::path PublicationProbePath =
        std::filesystem::path(Arguments[0]).parent_path() /
        "AssetCookerPublicationProbe";
    std::filesystem::path AssetCookerPath =
        std::filesystem::path(Arguments[0]).parent_path().parent_path() /
        "Tools" / "AssetCooker" / "StonerAssetCooker";
#if defined(_WIN32)
    LeaseProbePath += ".exe";
    PublicationProbePath += ".exe";
    AssetCookerPath += ".exe";
#endif
    for (int Index = 1; Index < ArgCount; ++Index)
    {
        const std::string Argument = Arguments[Index];
        if (Argument == "--emit-ktx2-dir" ||
            Argument == "--emit-ktx2-source-dir" ||
            Argument == "--report" ||
            Argument == "--ktx2-determinism-runs" ||
            Argument == "--material-shader-report" ||
            Argument == "--material-shader-determinism-runs" ||
            Argument == "--static-model-determinism-runs" ||
            Argument == "--static-model-performance-runs" ||
            Argument == "--static-model-performance-max-seconds" ||
            Argument == "--static-model-performance-fixture")
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
            else if (Argument == "--static-model-performance-fixture")
            {
                StaticModelOptions.PerformanceFixture = Value;
            }
            else if (Argument == "--static-model-performance-max-seconds")
            {
                errno = 0;
                char* End = nullptr;
                const double Seconds = std::strtod(Value.c_str(), &End);
                if (errno != 0 || End != Value.c_str() + Value.size() ||
                    !(Seconds > 0.0))
                {
                    std::cerr
                        << "Invalid --static-model-performance-max-seconds value\n";
                    return 2;
                }
                StaticModelOptions.PerformanceMaxSeconds = Seconds;
            }
            else if (Argument == "--static-model-determinism-runs" ||
                     Argument == "--static-model-performance-runs")
            {
                int Runs = 0;
                const auto Parsed = std::from_chars(
                    Value.data(), Value.data() + Value.size(), Runs);
                if (Parsed.ec != std::errc{} ||
                    Parsed.ptr != Value.data() + Value.size() ||
                    Runs <= 0 || Runs > 1000)
                {
                    std::cerr << "Invalid " << Argument << " value\n";
                    return 2;
                }
                if (Argument == "--static-model-determinism-runs")
                    StaticModelOptions.DeterminismRuns = Runs;
                else StaticModelOptions.PerformanceRuns = Runs;
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
        if (Argument == "--asset-cooker-benchmark-profile" ||
            Argument == "--asset-cooker-benchmark-corpus" ||
            Argument == "--asset-cooker-benchmark-report")
        {
            if (Index + 1 >= ArgCount)
            {
                std::cerr << "Missing value after " << Argument << '\n';
                return 2;
            }
            const std::string Value = Arguments[++Index];
            AssetCookerBenchmarkOptions.Enabled = true;
            if (Argument == "--asset-cooker-benchmark-profile")
            {
                if (Value != "reference" && Value != "ci")
                {
                    std::cerr << "Invalid --asset-cooker-benchmark-profile value\n";
                    return 2;
                }
                AssetCookerBenchmarkOptions.CiProfile = Value == "ci";
            }
            else if (Argument == "--asset-cooker-benchmark-corpus")
                AssetCookerBenchmarkOptions.Corpus = Value;
            else AssetCookerBenchmarkOptions.Report = Value;
            continue;
        }
        ParsedArguments.push_back(Argument);
    }

    FTestSuiteRegistry Registry;
    Registry.Register("application-scene", [] { return RunApplicationSceneEcsTests().Failed == 0 ? 0 : 1; });
    Registry.Register("application-window", [] { return RunApplicationWindowInputTests().Failed == 0 ? 0 : 1; });
    Registry.Register("asset", [KTX2Options, MaterialShaderOptions, StaticModelOptions] {
        return RunAssetTests(KTX2Options, MaterialShaderOptions, StaticModelOptions).Failed == 0 ? 0 : 1;
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
    Registry.Register("asset-static-model", [StaticModelOptions] {
        const auto Hierarchy = RunAssetStaticModelHierarchyTests();
        const auto Identity = RunAssetStaticModelIdentityTests();
        const auto Determinism =
            RunAssetStaticModelDeterminismTests(StaticModelOptions);
        const auto Concurrency = RunAssetStaticModelConcurrencyTests();
        const auto Benchmark = RunAssetStaticModelBenchmark(StaticModelOptions);
        return Hierarchy.Failed == 0 && Identity.Failed == 0 &&
            Determinism.Failed == 0 && Concurrency.Failed == 0 &&
            Benchmark.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-gltf-material", [] {
        const auto Material = RunAssetGLTFMaterialTests();
        const auto Image = RunAssetGLTFImageDependencyTests();
        return Material.Failed == 0 && Image.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-gltf-hardening", [] {
        const auto Malformed = RunAssetGLTFMalformedTests();
        const auto Resolver = RunAssetGLTFResolverTests();
        const auto Limit = RunAssetGLTFLimitTests();
        const auto Diagnostic = RunAssetGLTFDiagnosticTests();
        return Malformed.Failed == 0 && Resolver.Failed == 0 &&
            Limit.Failed == 0 && Diagnostic.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-gltf-malformed", [] {
        const auto Import = RunAssetGLTFMalformedTests();
        const auto Realization = RunRendererStaticMeshFailureTests();
        return Import.Failed == 0 && Realization.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-ktx2-encoder", [] {
        return RunAssetKTX2EncoderTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-profile", [] {
        return RunAssetCookerProfileTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-derived-key", [] {
        return RunAssetCookerDerivedKeyTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-codec", [] {
        const auto Payload = RunAssetCookerPayloadCodecTests();
        const auto Manifest = RunAssetCookerManifestTests();
        const auto Equivalence = RunAssetCookerEquivalenceTests();
        return Payload.Failed == 0 && Manifest.Failed == 0 &&
            Equivalence.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-graph", [] {
        return RunAssetCookerGraphTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-scheduler", [] {
        return RunAssetCookerSchedulerTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-snapshot", [] {
        return RunAssetCookerInputSnapshotTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-source-catalog", [] {
        return RunAssetCookerSourceCatalogTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-clean", [] {
        return RunAssetCookerDeterminismTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-determinism", [] {
        return RunAssetCookerDeterminismTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-ddc", [] {
        return RunAssetCookerDerivedDataTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-incremental", [] {
        return RunAssetCookerIncrementalTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-concurrency", [] {
        return RunAssetCookerConcurrencyTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-publication", [] {
        return RunAssetCookerPublicationTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-published-validation", [] {
        return RunAssetCookerPublishedValidationTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-publication-concurrency", [PublicationProbePath] {
        return RunAssetCookerPublicationConcurrencyTests(
            PublicationProbePath.string().c_str()).Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-target-profile", [] {
        return RunAssetCookerTargetProfileTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-profile-invalidation", [] {
        return RunAssetCookerProfileInvalidationTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-cli", [AssetCookerPath] {
        return RunAssetCookerCliTests(AssetCookerPath.string().c_str()).Failed == 0
            ? 0 : 1;
    });
    Registry.Register("asset-cooker-report", [] {
        return RunAssetCookerReportTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-workflow", [] {
        return RunAssetCookerWorkflowTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-cooker-benchmark", [AssetCookerBenchmarkOptions] {
        return RunAssetCookerBenchmark(AssetCookerBenchmarkOptions).Failed == 0
            ? 0 : 1;
    });
    Registry.Register("core-foundation", [] { return RunCoreFoundationTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-math", [] { return RunCoreMathTests().Failed == 0 ? 0 : 1; });
    Registry.Register("coordinate-convention", [] { return RunCoordinateConventionTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-platform", [] { return RunCorePlatformTests().Failed == 0 ? 0 : 1; });
    Registry.Register("core-file-transaction", [] {
        return RunCorePlatformFileTransactionTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("core-file-lease", [LeaseProbePath] {
        return RunCorePlatformFileLeaseTests(
            LeaseProbePath.string().c_str()).Failed == 0 ? 0 : 1;
    });
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
    Registry.Register("renderer-static-mesh", [] {
        const auto Success = RunRendererStaticMeshTests();
        const auto Failure = RunRendererStaticMeshFailureTests();
        return Success.Failed == 0 && Failure.Failed == 0 ? 0 : 1;
    });
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
