#include "ProductionImageAcceptanceTests.h"

#include "ProductionImageBaselineRegistry.h"
#include "ProductionNativeImageAcceptance.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
using namespace Stoner::Core;

void Record(FProductionImageAcceptanceTestResult& Result, bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FProductionCanonicalImage Image(uint32 Width, uint32 Height, FVector3 Value)
{
    FProductionCanonicalImage Result;
    Result.Width = Width;
    Result.Height = Height;
    Result.LinearRgb.resize(static_cast<usize>(Width) * Height * 3u);
    for (usize Pixel = 0; Pixel < Result.LinearRgb.size() / 3u; ++Pixel)
    {
        Result.LinearRgb[Pixel * 3u] = Value.X;
        Result.LinearRgb[Pixel * 3u + 1u] = Value.Y;
        Result.LinearRgb[Pixel * 3u + 2u] = Value.Z;
    }
    return Result;
}

void TestReadbackNormalization(FProductionImageAcceptanceTestResult& Result)
{
    const TArray<uint8> PaddedBgra = {
        0, 0, 255, 255, 0, 255, 0, 255, 99, 99, 99, 99,
        255, 0, 0, 255, 255, 255, 255, 255, 88, 88, 88, 88};
    FProductionCanonicalImage Canonical;
    FString Failure;
    const bool Ok = NormalizeProductionReadback(
        {PaddedBgra, 2, 2, 12, EProductionReadbackPixelFormat::BGRA8UNorm,
         EProductionImageOrigin::BottomLeft, EProductionColorTransfer::SRGB},
        Canonical, Failure);
    Record(Result, Ok && Canonical.IsValid() &&
        Canonical.LinearRgb[0] < 0.01f && Canonical.LinearRgb[2] > 0.99f &&
        Canonical.LinearRgb[6] > 0.99f && Canonical.LinearRgb[8] < 0.01f,
        "readback normalization removes pitch and canonicalizes channel/origin/transfer");

    const float Bad[] = {NAN, 0.0f, 0.0f, 1.0f};
    const auto* BadBytes = reinterpret_cast<const uint8*>(Bad);
    Record(Result, !NormalizeProductionReadback(
        {{BadBytes, sizeof(Bad)}, 1, 1, sizeof(Bad),
         EProductionReadbackPixelFormat::RGBA32Float,
         EProductionImageOrigin::TopLeft, EProductionColorTransfer::Linear},
        Canonical, Failure),
        "readback normalization rejects non-finite GPU values");

    const float SignedNormal[] = {-1.0f, 0.0f, 1.0f, 0.5f};
    const auto* SignedBytes = reinterpret_cast<const uint8*>(SignedNormal);
    Record(Result, NormalizeProductionSignedNormalReadback(
        {{SignedBytes, sizeof(SignedNormal)}, 1, 1, sizeof(SignedNormal),
         EProductionReadbackPixelFormat::RGBA32Float,
         EProductionImageOrigin::TopLeft, EProductionColorTransfer::Linear},
        Canonical, Failure) && Canonical.IsValid() &&
        Canonical.LinearRgb[0] == 0.0f &&
        Canonical.LinearRgb[1] == 0.5f &&
        Canonical.LinearRgb[2] == 1.0f,
        "signed world-space normal readback is encoded only inside validation");
    FVector3 Sample;
    Record(Result, SampleProductionReadbackPixel(
        {{SignedBytes, sizeof(SignedNormal)}, 1, 1, sizeof(SignedNormal),
         EProductionReadbackPixelFormat::RGBA32Float,
         EProductionImageOrigin::TopLeft, EProductionColorTransfer::Linear},
        0, 0, Sample, Failure) && Sample == FVector3(-1.0f, 0.0f, 1.0f),
        "validation can inspect a finite HDR or signed attachment pixel without clamping");
}

void TestSemanticProbeOrdering(FProductionImageAcceptanceTestResult& Result)
{
    auto Color = Image(4, 4, {0.0f, 0.0f, 0.0f});
    Color.LinearRgb[(1u * 4u + 2u) * 3u] = 0.8f;
    auto Normal = Image(4, 4, {0.5f, 0.5f, 1.0f});
    auto Depth = Image(4, 4, {1.0f, 1.0f, 1.0f});
    Depth.LinearRgb[0] = 0.25f;
    FProductionSemanticProbeRequest Request;
    Request.Color = &Color;
    Request.Normal = &Normal;
    Request.Depth = &Depth;
    Request.ExpectedFrameToken = 7;
    Request.ObservedFrameToken = 6;
    for (const char* Name : {"orientation", "primitive-material", "base-color",
             "normal-response", "metallic-roughness", "emissive", "marker"})
        Request.Regions.push_back({Name, 2, 1, {0.8f, 0.0f, 0.0f}, 0.01f});
    const auto Stale = RunProductionSemanticProbes(Request);
    Record(Result, !Stale.bPassed && Stale.FirstFailure == FString("current-frame"),
        "semantic probes reject stale frame before material regions and FLIP");

    Request.ObservedFrameToken = 7;
    Request.MinimumCoverageFraction = 0.01f;
    Request.MaximumCoverageFraction = 0.5f;
    Request.RequiredRegionNames = {"marker"};
    const auto Accepted = RunProductionSemanticProbes(Request);
    Record(Result, Accepted.bPassed && Accepted.PassedProbeCount >= 4,
        "semantic probes accept finite nonblank bounded coverage and expected region");

    Request.RequiredRegionNames = {"marker", "clearcoat"};
    const auto MissingMaterialRegion = RunProductionSemanticProbes(Request);
    Record(Result, !MissingMaterialRegion.bPassed &&
        MissingMaterialRegion.FirstFailure == FString("missing-region-clearcoat"),
        "semantic probes require every declared primitive/material region");
}

void TestFlipAndMutation(FProductionImageAcceptanceTestResult& Result)
{
    auto Reference = Image(16, 16, {0.25f, 0.5f, 0.75f});
    auto Same = Reference;
    FProductionFlipPolicy Exact{0.0001f, 0.0001f, 0.0001f, 0.01f, 0.0f};
    const auto Equal = CompareProductionImagesWithFlip(Reference, Same, Exact);
    Record(Result, Equal.bMeasured && Equal.bPassed && Equal.Maximum == 0.0f,
        "pinned CPU FLIP reports exact images as zero error");

    Same.LinearRgb[0] = 1.0f;
    Same.LinearRgb[1] = 0.0f;
    Same.LinearRgb[2] = 0.0f;
    const auto Mutated = CompareProductionImagesWithFlip(Reference, Same, Exact);
    Record(Result, Mutated.bMeasured && !Mutated.bPassed &&
        Mutated.Maximum > Exact.MaximumMax,
        "pinned CPU FLIP rejects an intentional image mutation");
}

void TestNativeEvidence(FProductionImageAcceptanceTestResult& Result)
{
    FProductionNativeImageEvidence Evidence{
        "metal", "metal", "native", "production-lantern-v1",
        "production-lantern-v1", true, true, true, true, true};
    FString Failure;
    Record(Result, ValidateProductionNativeImageEvidence(Evidence, Failure),
        "native proof accepts matching backend GPU readback and window-only capture");
    Evidence.ExecutedBackend = "vulkan";
    Record(Result, !ValidateProductionNativeImageEvidence(Evidence, Failure),
        "native proof rejects backend substitution");
    Evidence.ExecutedBackend = "metal";
    Evidence.bWindowOnlyCapture = false;
    Record(Result, !ValidateProductionNativeImageEvidence(Evidence, Failure),
        "native proof rejects a non-window capture");
}

void TestWorkloadRegions(FProductionImageAcceptanceTestResult& Result)
{
    TArray<FProductionRegionProbe> Regions;
    Record(Result, BuildProductionWorkloadRegions(
            "production-content-lantern-v2", 512, 512, Regions) &&
            Regions.size() == 7 && Regions[1].Name == FString("orientation"),
        "Lantern image acceptance selects its exact semantic regions");
    Record(Result,
        IsProductionWorkloadNormalProbeValid(
            "production-content-lantern-v2", {-1.0f, 0.0f, 0.0f}) &&
        !IsProductionWorkloadNormalProbeValid(
            "production-content-lantern-v2", {1.0f, 0.0f, 0.0f}),
        "Lantern v2 rejects the superseded opposite-facing surface");
    Record(Result, BuildProductionWorkloadRegions(
            "production-content-sponza-v2", 512, 512, Regions) &&
            Regions.size() == 7 && Regions[0].X == 486 && Regions[0].Y == 25 &&
            Regions[1].X == 51 && Regions[1].Y == 51,
        "Sponza image acceptance selects its exact semantic regions");
    Record(Result,
        IsProductionWorkloadNormalProbeValid(
            "production-content-sponza-v2", {0.0f, 1.0f, 0.0f}) &&
        !IsProductionWorkloadNormalProbeValid(
            "production-content-sponza-v2", {0.0f, -1.0f, 0.0f}) &&
        !IsProductionWorkloadNormalProbeValid(
            "production-content-unknown-v1", {0.0f, 1.0f, 0.0f}),
        "Sponza image acceptance rejects the opposite-facing world normal");
    Record(Result, !BuildProductionWorkloadRegions(
            "production-content-unknown-v1", 512, 512, Regions) &&
            Regions.empty(),
        "image acceptance rejects an undeclared workload region contract");
}

void WriteText(const std::filesystem::path& Path, const std::string& Text)
{
    std::filesystem::create_directories(Path.parent_path());
    std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
    Output << Text;
}

std::string SignatureJson()
{
    return R"({"registryVersion":1,"backendImplementation":"native-metal","cpuArchitecture":"arm64","adapterFamily":"apple8","shaderProfile":"metal-macos-12-arm64","colorFormat":"rgba8-unorm","depthFormat":"d32-float","sampleCount":1,"textureFormatFamily":"astc"})";
}

std::string BaselineJson(const char* State, const char* Id)
{
    return std::string(R"({"schema":"stoner.production-image-baseline","schemaVersion":1,"baselineId":")") +
        Id + R"(","state":")" + State +
        R"(","workloadRevision":"production-lantern-v1","backend":"metal","deviceClass":"macos.apple8.metal.rgba8","capabilitySignature":)" +
        SignatureJson() +
        R"(,"width":64,"height":64,"colorTransfer":"srgb","referencePath":"lantern.ppm","referenceSha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","flipPolicy":{"meanMax":0.01,"p95Max":0.02,"maximumMax":0.1,"badPixelThreshold":0.05,"badPixelFractionMax":0.01},"calibrationEvidenceSha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"})";
}

void TestBaselineRegistry(FProductionImageAcceptanceTestResult& Result)
{
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-production-image-registry-tests";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    const auto RegistryPath = Root / "DeviceClasses.json";
    WriteText(RegistryPath,
        std::string(R"({"schema":"stoner.production-device-class-registry","schemaVersion":1,"registryVersion":1,"classes":[{"deviceClass":"macos.apple8.metal.rgba8","capabilitySignature":)") +
        SignatureJson() + "}]}");
    WriteText(Root / "accepted.json", BaselineJson("accepted", "lantern-metal"));

    FProductionImageBaselineRegistry Registry;
    FString Failure;
    FProductionCapabilitySignature Signature{
        1, "native-metal", "arm64", "apple8", "metal-macos-12-arm64",
        "rgba8-unorm", "d32-float", 1, "astc"};
    FProductionImageBaseline Baseline;
    Record(Result,
        Registry.LoadDeviceClasses(FString(RegistryPath.string()), Failure) &&
        Registry.LoadBaselines(FString(Root.string()), Failure) &&
        Registry.SelectAccepted(Signature, "production-lantern-v1", "metal",
            Baseline, Failure) && Baseline.State == FString("accepted"),
        "baseline selection derives one exact device class and accepted record");

    Signature.AdapterFamily = "apple9";
    Record(Result,
        !Registry.SelectAccepted(Signature, "production-lantern-v1", "metal",
            Baseline, Failure) && Failure == FString("device-class-missing"),
        "baseline selection has no nearest or fallback device class");

    WriteText(Root / "accepted.json", BaselineJson("candidate", "lantern-metal"));
    FProductionImageBaselineRegistry CandidateRegistry;
    Signature.AdapterFamily = "apple8";
    Record(Result,
        CandidateRegistry.LoadDeviceClasses(FString(RegistryPath.string()), Failure) &&
        CandidateRegistry.LoadBaselines(FString(Root.string()), Failure) &&
        !CandidateRegistry.SelectAccepted(Signature,
            "production-lantern-v1", "metal", Baseline, Failure) &&
        Failure == FString("baseline-state-not-accepted"),
        "baseline selection rejects every non-accepted lifecycle state");
    std::filesystem::remove_all(Root, Error);
}

} // namespace

FProductionImageAcceptanceTestResult RunProductionImageAcceptanceTests()
{
    FProductionImageAcceptanceTestResult Result;
    TestReadbackNormalization(Result);
    TestSemanticProbeOrdering(Result);
    TestFlipAndMutation(Result);
    TestNativeEvidence(Result);
    TestWorkloadRegions(Result);
    TestBaselineRegistry(Result);
    return Result;
}
