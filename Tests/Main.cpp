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
#include "CorePlatformProcessTests.h"
#include "LoggingAssertionTests.h"
#include "MetalTests.h"
#include "MetalBackendComparisonTests.h"
#include "MetalDeviceTests.h"
#include "MetalDiagnosticsTests.h"
#include "MetalFailureInjectionTests.h"
#include "MetalLifecycleStressTests.h"
#include "MetalCommandTests.h"
#include "MetalNativeIntegrationTests.h"
#include "MetalPipelineTests.h"
#include "MetalPresentationIntegrationTests.h"
#include "MetalPresentationTests.h"
#include "MetalResourceTests.h"
#include "MetalShaderCompilerTests.h"
#include "MetalShaderCookerTests.h"
#include "MetalShaderDerivationTests.h"
#include "MetalShaderPublicationTests.h"
#include "MetalShaderRuntimeTests.h"
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
#include "AssetManagerCookedTests.h"
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
    FAssetManagerTestOptions AssetManagerOptions;
    FMetalTestOptions MetalOptions;
    std::vector<std::string> ParsedArguments;
    std::filesystem::path LeaseProbePath =
        std::filesystem::path(Arguments[0]).parent_path() /
        "PlatformFileLeaseProbe";
    std::filesystem::path ProcessProbePath =
        std::filesystem::path(Arguments[0]).parent_path() /
        "PlatformProcessProbe";
    std::filesystem::path PublicationProbePath =
        std::filesystem::path(Arguments[0]).parent_path() /
        "AssetCookerPublicationProbe";
    std::filesystem::path GenerationLeaseProbePath =
        std::filesystem::path(Arguments[0]).parent_path() /
        "GenerationReaderLeaseProbe";
    std::filesystem::path AssetCookerPath =
        std::filesystem::path(Arguments[0]).parent_path().parent_path() /
        "Tools" / "AssetCooker" / "StonerAssetCooker";
#if defined(_WIN32)
    LeaseProbePath += ".exe";
    ProcessProbePath += ".exe";
    PublicationProbePath += ".exe";
    GenerationLeaseProbePath += ".exe";
    AssetCookerPath += ".exe";
#endif
    AssetManagerOptions.GenerationLeaseProbe =
        GenerationLeaseProbePath.string();
    for (int Index = 1; Index < ArgCount; ++Index)
    {
        const std::string Argument = Arguments[Index];
        if (Argument == "--emit-ktx2-dir" ||
            Argument == "--emit-ktx2-source-dir" ||
            Argument == "--report" ||
            Argument == "--ktx2-determinism-runs" ||
            Argument == "--material-shader-report" ||
            Argument == "--material-shader-determinism-runs" ||
            Argument == "--metal-report" ||
            Argument == "--metal-cooked-root" ||
            Argument == "--metal-lease-root" ||
            Argument == "--metal-target-profile" ||
            Argument == "--metal-determinism-runs" ||
            Argument == "--metal-lifecycle-iterations" ||
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
            else if (Argument == "--metal-report")
            {
                MetalOptions.ReportPath = Value;
            }
            else if (Argument == "--metal-cooked-root")
            {
                MetalOptions.CookedPublicationRoot = Value;
            }
            else if (Argument == "--metal-lease-root")
            {
                MetalOptions.LeaseCoordinationRoot = Value;
            }
            else if (Argument == "--metal-target-profile")
            {
                MetalOptions.TargetProfilePath = Value;
            }
            else if (Argument == "--metal-determinism-runs" ||
                     Argument == "--metal-lifecycle-iterations")
            {
                std::uint32_t Count = 0;
                const auto Parsed = std::from_chars(
                    Value.data(), Value.data() + Value.size(), Count);
                const std::uint32_t Maximum =
                    Argument == "--metal-determinism-runs" ? 1000u : 1000000u;
                if (Parsed.ec != std::errc{} ||
                    Parsed.ptr != Value.data() + Value.size() ||
                    Count == 0 || Count > Maximum)
                {
                    std::cerr << "Invalid " << Argument << " value\n";
                    return 2;
                }
                if (Argument == "--metal-determinism-runs")
                    MetalOptions.DeterminismRuns = Count;
                else MetalOptions.LifecycleIterations = Count;
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
        if (Argument == "--asset-manager-benchmark-profile" ||
            Argument == "--asset-manager-benchmark-report")
        {
            if (Index + 1 >= ArgCount)
            {
                std::cerr << "Missing value after " << Argument << '\n';
                return 2;
            }
            const std::string Value = Arguments[++Index];
            AssetManagerOptions.BenchmarkEnabled = true;
            if (Argument == "--asset-manager-benchmark-profile")
            {
                if (Value != "reference" && Value != "ci")
                {
                    std::cerr
                        << "Invalid --asset-manager-benchmark-profile value\n";
                    return 2;
                }
                AssetManagerOptions.BenchmarkCiProfile = Value == "ci";
            }
            else AssetManagerOptions.BenchmarkReport = Value;
            continue;
        }
        if (Argument == "--metal-native")
        {
            MetalOptions.bRequestNative = true;
            continue;
        }
        if (Argument == "--metal-visible")
        {
            MetalOptions.bRequestVisible = true;
            continue;
        }
        ParsedArguments.push_back(Argument);
    }

    FTestSuiteRegistry Registry;
    Registry.Register("application-scene", [] { return RunApplicationSceneEcsTests().Failed == 0 ? 0 : 1; });
    Registry.Register("application-window", [] { return RunApplicationWindowInputTests().Failed == 0 ? 0 : 1; });
    Registry.Register("asset", [KTX2Options, MaterialShaderOptions, StaticModelOptions, AssetManagerOptions] {
        return RunAssetTests(KTX2Options, MaterialShaderOptions, StaticModelOptions, AssetManagerOptions).Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager", [AssetManagerOptions] {
        return RunAssetManagerTests(AssetManagerOptions).Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-request", [] {
        return RunAssetManagerKernelTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-cooked", [] {
        return RunAssetManagerCookedTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-contract", [] {
        return RunAssetManagerContractTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-dependency", [] {
        return RunAssetManagerDependencyTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-development", [] {
        return RunAssetManagerDevelopmentTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-source-mutation", [] {
        return RunAssetManagerDevelopmentTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-equivalence", [] {
        return RunAssetManagerEquivalenceTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-cache", [] {
        return RunAssetManagerCacheTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-completion", [] {
        return RunAssetManagerCompletionTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-lifetime", [] {
        return RunAssetManagerLifetimeTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-cancellation", [] {
        return RunAssetManagerCancellationTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-shutdown", [] {
        return RunAssetManagerShutdownTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-inspection", [] {
        return RunAssetManagerInspectionTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-stress", [] {
        return RunAssetManagerStressTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-generation-lease", [AssetManagerOptions] {
        const auto Local = RunAssetManagerGenerationLeaseTests();
        const auto Process = RunAssetManagerGenerationLeaseProcessTests(
            AssetManagerOptions.GenerationLeaseProbe.c_str());
        return Local.Failed == 0 && Process.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-generation-lease-process", [AssetManagerOptions] {
        return RunAssetManagerGenerationLeaseProcessTests(
            AssetManagerOptions.GenerationLeaseProbe.c_str()).Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-concurrency", [] {
        const auto Coalescing = RunAssetManagerCoalescingTests();
        const auto Cancellation = RunAssetManagerCancellationTests();
        const auto Shutdown = RunAssetManagerShutdownTests();
        return Coalescing.Failed == 0 && Cancellation.Failed == 0 &&
            Shutdown.Failed == 0 ? 0 : 1;
    });
    Registry.Register("asset-manager-benchmark", [AssetManagerOptions] {
        return RunAssetManagerBenchmark(
            AssetManagerOptions.BenchmarkEnabled,
            AssetManagerOptions.BenchmarkCiProfile,
            AssetManagerOptions.BenchmarkReport).Failed == 0 ? 0 : 1;
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
    Registry.Register("core-platform-process", [ProcessProbePath] {
        return RunCorePlatformProcessTests(
            ProcessProbePath.string().c_str()).Failed == 0 ? 0 : 1;
    });
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
    Registry.Register("metal", [MetalOptions] {
        const auto Scaffold = RunMetalTests(MetalOptions);
        const auto Device = RunMetalDeviceTests();
        const auto Diagnostics = RunMetalDiagnosticsTests();
        const auto Failure = RunMetalFailureInjectionTests();
        const auto Lifecycle = RunMetalLifecycleStressTests(MetalOptions);
        const auto Command = RunMetalCommandTests();
        const auto Pipeline = RunMetalPipelineTests();
        const auto Presentation = RunMetalPresentationTests();
        const auto Visible = RunMetalPresentationIntegrationTests(
            MetalOptions.bRequestVisible);
        const auto Native = RunMetalNativeIntegrationTests(
            MetalOptions.bRequestNative);
        const auto Resource = RunMetalResourceTests();
        const auto Derivation = RunMetalShaderDerivationTests();
        const auto Compiler = RunMetalShaderCompilerTests();
        const auto Cooker = RunMetalShaderCookerTests();
        const auto Publication = RunMetalShaderPublicationTests();
        const auto Runtime = RunMetalShaderRuntimeTests();
        return Scaffold.Failed == 0 && Device.Failed == 0 &&
            Diagnostics.Failed == 0 && Failure.Failed == 0 &&
            Lifecycle.Failed == 0 &&
            Command.Failed == 0 &&
            Pipeline.Failed == 0 && Presentation.Failed == 0 &&
            Visible.Failed == 0 &&
            Native.Failed == 0 &&
            Resource.Failed == 0 && Derivation.Failed == 0 &&
            Compiler.Failed == 0 && Cooker.Failed == 0 &&
            Publication.Failed == 0 && Runtime.Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-backend-comparison", [] {
        return RunMetalBackendComparisonTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-device", [] {
        return RunMetalDeviceTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-diagnostics", [] {
        return RunMetalDiagnosticsTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-failure-injection", [] {
        return RunMetalFailureInjectionTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-lifecycle-stress", [MetalOptions] {
        return RunMetalLifecycleStressTests(MetalOptions).Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-command", [] {
        return RunMetalCommandTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-pipeline", [] {
        return RunMetalPipelineTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-presentation", [] {
        return RunMetalPresentationTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-presentation-visible", [MetalOptions] {
        return RunMetalPresentationIntegrationTests(
            MetalOptions.bRequestVisible).Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-native", [MetalOptions] {
        return RunMetalNativeIntegrationTests(
            MetalOptions.bRequestNative).Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-resource", [] {
        return RunMetalResourceTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-shader-compiler", [] {
        return RunMetalShaderCompilerTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-shader-cooker", [] {
        return RunMetalShaderCookerTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-shader-derivation", [] {
        return RunMetalShaderDerivationTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-shader-publication", [] {
        return RunMetalShaderPublicationTests().Failed == 0 ? 0 : 1;
    });
    Registry.Register("metal-shader-runtime", [MetalOptions] {
        return RunMetalShaderRuntimeTests(MetalOptions).Failed == 0 ? 0 : 1;
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
