#include "ProductionImageAcceptanceTests.h"

#include "ProductionImageBaselineRegistry.h"
#include "ProductionImageReference.h"
#include "ProductionNativeImageAcceptance.h"
#include "FStonerDemoApplication.h"

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
    auto Color = Image(8, 8, {0.0f, 0.0f, 0.0f});
    for (uint32 Y = 2; Y < 6; ++Y)
    {
        for (uint32 X = 2; X < 6; ++X)
        {
            if (X == 2) continue;
            const usize Pixel = (static_cast<usize>(Y) * 8u + X) * 3u;
            Color.LinearRgb[Pixel] = 0.8f;
        }
    }
    auto Normal = Image(8, 8, {0.5f, 0.5f, 1.0f});
    auto Depth = Image(8, 8, {1.0f, 1.0f, 1.0f});
    Depth.LinearRgb[0] = 0.25f;
    FProductionSemanticProbeRequest Request;
    Request.Color = &Color;
    Request.Normal = &Normal;
    Request.Depth = &Depth;
    Request.ExpectedFrameToken = 7;
    Request.ObservedFrameToken = 6;
    for (const char* Name : {"orientation", "primitive-material", "base-color",
             "normal-response", "metallic-roughness", "emissive", "marker"})
        Request.Regions.push_back({Name, {2, 2, 6, 6},
            {0.8f, 0.0f, 0.0f}, 0.01f, 0.70f,
            EProductionRegionStatistic::Median, 0.5f});
    const auto Stale = RunProductionSemanticProbes(Request);
    Record(Result, !Stale.bPassed && Stale.FirstFailure == FString("current-frame"),
        "semantic probes reject stale frame before material regions and FLIP");

    Request.ObservedFrameToken = 7;
    Request.MinimumCoverageFraction = 0.01f;
    Request.MaximumCoverageFraction = 0.5f;
    Request.RequiredRegionNames = {"marker"};
    const auto Accepted = RunProductionSemanticProbes(Request);
    const TArray<FString> ExpectedProbeIds = {
        "color-image", "current-frame", "nonblank", "coverage",
        "region-orientation", "region-primitive-material",
        "region-base-color", "region-normal-response",
        "region-metallic-roughness", "region-emissive", "region-marker",
        "normal-semantic", "depth-semantic"};
    Record(Result, Accepted.bPassed &&
        Accepted.PassedProbeCount == ExpectedProbeIds.size() &&
        Accepted.PassedProbeIds == ExpectedProbeIds,
        "semantic probes accept bounded majority coverage and robust region medians");

    Color.LinearRgb[(2u * 8u + 2u) * 3u] = 1.0f;
    const auto EdgePerturbation = RunProductionSemanticProbes(Request);
    Record(Result, EdgePerturbation.bPassed,
        "semantic region probes tolerate one primitive-edge coverage change");

    Request.RequiredRegionNames = {"marker", "clearcoat"};
    const auto MissingMaterialRegion = RunProductionSemanticProbes(Request);
    Record(Result, !MissingMaterialRegion.bPassed &&
        MissingMaterialRegion.FirstFailure == FString("missing-region-clearcoat"),
        "semantic probes require every declared primitive/material region");
}

void TestReadbackRegionStatistics(FProductionImageAcceptanceTestResult& Result)
{
    TArray<float> Values(4u * 4u * 4u, 0.0f);
    for (uint32 Pixel = 0; Pixel < 16; ++Pixel)
    {
        Values[Pixel * 4u] = Pixel < 12 ? 0.75f : 0.0f;
        Values[Pixel * 4u + 1u] = Pixel < 12 ? 0.25f : 0.0f;
        Values[Pixel * 4u + 2u] = Pixel < 12 ? 0.50f : 0.0f;
        Values[Pixel * 4u + 3u] = 1.0f;
    }
    const auto* Bytes = reinterpret_cast<const uint8*>(Values.data());
    const FProductionReadbackView View{
        {Bytes, Values.size() * sizeof(float)}, 4, 4,
        4u * 4u * sizeof(float), EProductionReadbackPixelFormat::RGBA32Float,
        EProductionImageOrigin::TopLeft, EProductionColorTransfer::Linear};
    FProductionReadbackRegionSample Sample;
    FString Failure;
    Record(Result, SampleProductionReadbackRegion(
        View, {0, 0, 4, 4}, 0.5f, Sample, Failure) &&
        std::abs(Sample.Value.X - 0.75f) < 0.0001f &&
        Sample.ValidSampleFraction == 1.0f,
        "attachment regions expose a bounded median and valid-sample fraction");

    TArray<float> Normals(4u * 4u * 4u, 0.0f);
    for (uint32 Pixel = 0; Pixel < 16; ++Pixel)
    {
        Normals[Pixel * 4u] = Pixel < 12 ? 0.0f : 1.0f;
        Normals[Pixel * 4u + 1u] = Pixel < 12 ? 1.0f : 0.0f;
        Normals[Pixel * 4u + 2u] = 0.0f;
        Normals[Pixel * 4u + 3u] = 1.0f;
    }
    Bytes = reinterpret_cast<const uint8*>(Normals.data());
    const FProductionReadbackView NormalView{
        {Bytes, Normals.size() * sizeof(float)}, 4, 4,
        4u * 4u * sizeof(float), EProductionReadbackPixelFormat::RGBA32Float,
        EProductionImageOrigin::TopLeft, EProductionColorTransfer::Linear};
    float Coverage = 0.0f;
    Record(Result, MeasureProductionReadbackDirectionalCoverage(
        NormalView, {0, 0, 4, 4}, {0.0f, 1.0f, 0.0f}, 0.8f,
        Coverage, Failure) && std::abs(Coverage - 0.75f) < 0.0001f,
        "normal attachment regions measure directional sample coverage");
    Record(Result, MeasureProductionReadbackDirectionalCoverage(
        NormalView, {0, 0, 4, 4}, {0.0f, -1.0f, 0.0f}, 0.8f,
        Coverage, Failure) && Coverage < 0.60f,
        "normal attachment gate rejects the opposite-direction mutation");
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

    Reference = Image(16, 16, {0.0f, 0.0f, 0.0f});
    for (uint32 Y = 4; Y < 12; ++Y)
        for (uint32 X = 4; X < 12; ++X)
            Reference.LinearRgb[(static_cast<usize>(Y) * 16u + X) * 3u] = 1.0f;
    auto Translated = Image(16, 16, {0.0f, 0.0f, 0.0f});
    for (uint32 Y = 4; Y < 12; ++Y)
        for (uint32 X = 5; X < 13; ++X)
            Translated.LinearRgb[(static_cast<usize>(Y) * 16u + X) * 3u] = 1.0f;
    const auto Shifted = CompareProductionImagesWithFlip(
        Reference, Translated, Exact);
    Record(Result, Shifted.bMeasured && !Shifted.bPassed,
        "exact-coordinate FLIP rejects a one-pixel whole-image translation");
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

void TestAuthoritativeFrameBundle(FProductionImageAcceptanceTestResult& Result)
{
    Stoner::Demo::FDemoProductionExecutionInspection Inspection;
    constexpr uint64 FrameToken = 41;
    for (const char* Name : {
             "FinalOutput", "BaseColorAO", "NormalRoughness",
             "EmissiveMetallic", "Depth", "LightingAccumulation"})
    {
        Stoner::Demo::FDemoProductionReadbackEvidence Evidence;
        Evidence.Name = Name;
        Evidence.FrameToken = FrameToken;
        Inspection.Readbacks.push_back(std::move(Evidence));
    }
    Inspection.AuthoritativeFrameToken = FrameToken;
    Inspection.LastLifecyclePresentedFrameToken = FrameToken - 1u;
    Inspection.AuthoritativeCapture.FrameToken = FrameToken;
    Inspection.AuthoritativeCapture.ExpectedFrameToken = FrameToken;
    Inspection.AuthoritativeCapture.bPresented = true;
    Inspection.AuthoritativeCapture.bWindowOnlyCapture = true;
    Inspection.AuthoritativeCapture.Bytes = {1, 2, 3, 4};
    Stoner::Demo::FDemoProductionCapture Stale;
    Stale.FrameToken = FrameToken - 1u;
    Stale.ExpectedFrameToken = FrameToken - 1u;
    Stale.bPresented = true;
    Stale.bWindowOnlyCapture = true;
    Stale.Bytes = {1, 2, 3, 4};
    Inspection.LastLifecyclePresentedCapture = std::move(Stale);
    FString Failure;
    Record(Result,
        ValidateProductionAuthoritativeFrameBundle(Inspection, Failure),
        "authoritative attachments and window capture accept one submission token");

    Inspection.Readbacks[2].FrameToken = FrameToken - 1u;
    Record(Result,
        !ValidateProductionAuthoritativeFrameBundle(Inspection, Failure) &&
            Failure == FString("authoritative-frame-bundle"),
        "authoritative frame bundle rejects one stale attachment");
    Inspection.Readbacks[2].FrameToken = FrameToken;
    Inspection.AuthoritativeCapture.FrameToken = FrameToken - 1u;
    Record(Result,
        !ValidateProductionAuthoritativeFrameBundle(Inspection, Failure) &&
            Failure == FString("authoritative-frame-bundle"),
        "authoritative frame bundle rejects a stale window capture");
    Inspection.AuthoritativeCapture.FrameToken = FrameToken;
    Inspection.AuthoritativeCapture.ExpectedFrameToken = FrameToken - 1u;
    Record(Result,
        !ValidateProductionAuthoritativeFrameBundle(Inspection, Failure) &&
            Failure == FString("authoritative-frame-bundle"),
        "authoritative frame bundle rejects a self-reported expected-token mismatch");
    Inspection.AuthoritativeCapture.ExpectedFrameToken = FrameToken;
    Inspection.LastLifecyclePresentedCapture = {};
    Record(Result,
        !ValidateProductionAuthoritativeFrameBundle(Inspection, Failure) &&
            Failure == FString("stale-frame-bundle-evidence"),
        "authoritative frame bundle requires a real prior presented token");
}

void TestReferenceSetSelection(FProductionImageAcceptanceTestResult& Result)
{
    FProductionReferenceComparison First;
    First.ReferenceId = "alpha";
    First.Flip.bMeasured = true;
    FProductionReferenceComparison Second;
    Second.ReferenceId = "beta";
    Second.Flip.bMeasured = true;
    Second.Flip.bPassed = true;
    Second.Flip.Mean = 0.001f;
    FString Matched;
    FProductionFlipResult Flip;
    Record(Result,
        SelectProductionMatchedReference(
            {First, Second}, Matched, Flip) &&
            Matched == FString("beta") && Flip.Mean == 0.001f,
        "reference set accepts the canonical second reference when the first fails");
    Second.Flip.bPassed = false;
    Record(Result,
        !SelectProductionMatchedReference(
            {First, Second}, Matched, Flip) && Matched.IsEmpty(),
        "reference set fails closed when every reference rejects the candidate");
}

void TestWorkloadRegions(FProductionImageAcceptanceTestResult& Result)
{
    TArray<FProductionRegionProbe> Regions;
    Record(Result, BuildProductionWorkloadRegions(
            "production-content-lantern-v2", 512, 512, Regions) &&
            Regions.size() == 7 && Regions[1].Name == FString("orientation") &&
            Regions[1].Region.MaximumXExclusive >
                Regions[1].Region.MinimumX,
        "Lantern image acceptance selects bounded semantic regions at the canonical extent");
    Record(Result,
        IsProductionWorkloadNormalProbeValid(
            "production-content-lantern-v2", {-1.0f, 0.0f, 0.0f}) &&
        IsProductionWorkloadNormalProbeValid(
            "production-content-lantern-v3", {-1.0f, 0.0f, 0.0f}) &&
        !IsProductionWorkloadNormalProbeValid(
            "production-content-lantern-v2", {1.0f, 0.0f, 0.0f}),
        "Lantern v2/v3 reject the superseded opposite-facing surface");
    Record(Result, BuildProductionWorkloadRegions(
            "production-content-sponza-v2", 512, 512, Regions) &&
            Regions.size() == 7 && Regions[0].Region.MinimumX < 486 &&
            Regions[0].Region.MaximumXExclusive > 486 &&
            Regions[1].Region.MinimumX < 51 &&
            Regions[1].Region.MaximumXExclusive > 51,
        "Sponza image acceptance selects workload-versioned bounded regions");
    Record(Result,
        IsProductionWorkloadNormalProbeValid(
            "production-content-sponza-v2", {0.0f, 1.0f, 0.0f}) &&
        IsProductionWorkloadNormalProbeValid(
            "production-content-sponza-v3", {0.0f, 1.0f, 0.0f}) &&
        !IsProductionWorkloadNormalProbeValid(
            "production-content-sponza-v2", {0.0f, -1.0f, 0.0f}) &&
        !IsProductionWorkloadNormalProbeValid(
            "production-content-unknown-v1", {0.0f, 1.0f, 0.0f}),
        "Sponza image acceptance rejects the opposite-facing world normal");
    Record(Result, !BuildProductionWorkloadRegions(
            "production-content-unknown-v1", 512, 512, Regions) &&
            Regions.empty(),
        "image acceptance rejects an undeclared workload region contract");
    Record(Result, !BuildProductionWorkloadRegions(
            "production-content-lantern-v2", 256, 256, Regions) &&
            Regions.empty(),
        "formal workload region contracts reject non-canonical extents");
}

void TestAcceptedReferenceRegionCalibration(
    FProductionImageAcceptanceTestResult& Result)
{
    const auto Validate = [](const char* Workload, const char* Relative,
        float MinimumCoverage, float MaximumCoverage)
    {
        FProductionCanonicalImage Color;
        FString Failure;
        if (!LoadProductionReferenceImage(
                Relative, EProductionColorTransfer::SRGB,
                Color, Failure))
            return false;
        auto Normal = Image(512, 512, {0.5f, 0.5f, 1.0f});
        auto Depth = Image(512, 512, {1.0f, 1.0f, 1.0f});
        Depth.LinearRgb[0] = 0.25f;
        FProductionSemanticProbeRequest Request;
        Request.Color = &Color;
        Request.Normal = &Normal;
        Request.Depth = &Depth;
        Request.ExpectedFrameToken = 20;
        Request.ObservedFrameToken = 20;
        Request.MinimumCoverageFraction = MinimumCoverage;
        Request.MaximumCoverageFraction = MaximumCoverage;
        Request.RequiredRegionNames = {"background"};
        return BuildProductionWorkloadRegions(
                Workload, 512, 512, Request.Regions) &&
            RunProductionSemanticProbes(Request).bPassed;
    };
    Record(Result,
        Validate("production-content-lantern-v2",
            "Content/ProductionAcceptance/Baselines/"
            "macos.apple8.metal.rgba8/production-content-lantern-v2.png",
            0.01f, 0.35f) &&
        Validate("production-content-lantern-v2",
            "Content/ProductionAcceptance/Baselines/"
            "windows.discrete-vulkan.rgba8/production-content-lantern-v2.png",
            0.01f, 0.35f) &&
        Validate("production-content-sponza-v2",
            "Content/ProductionAcceptance/Baselines/"
            "macos.apple8.metal.rgba8/production-content-sponza-v2.png",
            0.75f, 0.82f),
        "accepted Lantern v2 and Sponza v2 references pass recalibrated region semantics");
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
    const std::string Reference =
        R"({"referenceId":"primary","referencePath":"lantern.png","referenceSha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","flipPolicy":{"meanMax":0.01,"p95Max":0.02,"maximumMax":0.1,"badPixelThreshold":0.05,"badPixelFractionMax":0.01},"calibrationEvidenceSha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"})";
    return std::string(R"({"schema":"stoner.production-image-baseline","schemaVersion":2,"baselineId":")") +
        Id + R"(","state":")" + State +
        R"(","workloadRevision":"production-lantern-v1","backend":"metal","deviceClass":"macos.apple8.metal.rgba8","capabilitySignature":)" +
        SignatureJson() +
        R"(,"width":512,"height":512,"colorTransfer":"srgb","references":[)" +
        Reference + "]}";
}

std::string ReferenceJson(
    const char* Id,
    const char* Path,
    char ReferenceDigest,
    char CalibrationDigest)
{
    return std::string(R"({"referenceId":")") + Id +
        R"(","referencePath":")" + Path +
        R"(","referenceSha256":")" + std::string(64, ReferenceDigest) +
        R"(","flipPolicy":{"meanMax":0.01,"p95Max":0.02,"maximumMax":0.1,"badPixelThreshold":0.05,"badPixelFractionMax":0.01},"calibrationEvidenceSha256":")" +
        std::string(64, CalibrationDigest) + R"("})";
}

std::string BaselineJsonWithReferences(
    const char* State,
    const char* Id,
    const std::string& References)
{
    std::string Result = BaselineJson(State, Id);
    const auto Begin = Result.find("[{");
    const auto End = Result.rfind("]}");
    Result.replace(Begin + 1u, End - Begin - 1u, References);
    return Result;
}

std::string SdrBaselineV3Json(
    const char* State,
    const char* Acceptance,
    const char* Id = "lantern-metal-srgb-neutral-v3")
{
    return std::string(R"({"schema":"stoner.sdr-image-baseline","schemaVersion":3,"baselineId":")") +
        Id + R"(","state":")" + State +
        R"(","workloadRevision":"production-content-lantern-v3","backend":"metal","deviceClass":"macos.apple8.metal.sdr","capabilityDigest":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","outputDeviceProfileId":"Sdr.sRGB.v1","transformVersion":"Sdr.KhronosPbrNeutral.v1","exposureStops":0,"settingsDigest":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","width":512,"height":512,"sampleCount":1,"referencePath":"Validation/029/SDR/M4-Metal/lantern-v3.png","compressedSha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","decodedSha256":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","calibrationEvidenceSha256":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","flipPolicy":{"meanMax":0.01,"p95Max":0.02,"maximumMax":0.1,"badPixelThreshold":0.05,"badPixelFractionMax":0.01},"acceptance":)" +
        Acceptance + "}";
}

std::string SdrBaselineV3Registry(const std::string& Records)
{
    return std::string(R"({"schema":"stoner.sdr-image-baseline-registry","schemaVersion":3,"registryId":"output-transform-sdr-baselines-v3","records":[)") +
        Records + "]}";
}

void TestSdrBaselineV3Registry(FProductionImageAcceptanceTestResult& Result)
{
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-output-transform-sdr-v3-registry-tests";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    const auto RegistryPath = Root / "Baselines-v3.json";
    const std::string Acceptance =
        R"({"maintainerId":"maintainer","reviewedAt":"2026-09-02T12:00:00Z","candidateSha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","decision":"accepted"})";

    Record(Result,
        FOutputTransformSdrBaselineRegistryV3::IsAllowedStateTransition(
            "candidate", "calibrated") &&
        FOutputTransformSdrBaselineRegistryV3::IsAllowedStateTransition(
            "calibrated", "reviewed") &&
        FOutputTransformSdrBaselineRegistryV3::IsAllowedStateTransition(
            "reviewed", "accepted") &&
        FOutputTransformSdrBaselineRegistryV3::IsAllowedStateTransition(
            "accepted", "superseded") &&
        !FOutputTransformSdrBaselineRegistryV3::IsAllowedStateTransition(
            "candidate", "accepted") &&
        !FOutputTransformSdrBaselineRegistryV3::IsAllowedStateTransition(
            "superseded", "accepted"),
        "SDR v3 baseline lifecycle permits only the declared forward transitions");

    FString Failure;
    FOutputTransformSdrBaselineV3 Selected;
    WriteText(RegistryPath, SdrBaselineV3Registry(
        SdrBaselineV3Json("candidate", "null")));
    FOutputTransformSdrBaselineRegistryV3 CandidateRegistry;
    Record(Result,
        CandidateRegistry.LoadRegistry(FString(RegistryPath.string()), Failure) &&
        !CandidateRegistry.SelectAccepted(
            "production-content-lantern-v3", "metal",
            "macos.apple8.metal.sdr", "Sdr.sRGB.v1",
            "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            Selected, Failure) &&
        Failure == FString("sdr-v3-state-not-accepted"),
        "ordinary SDR v3 Candidate loading cannot promote or select authority");

    WriteText(RegistryPath, SdrBaselineV3Registry(
        SdrBaselineV3Json("accepted", "null")));
    FOutputTransformSdrBaselineRegistryV3 MissingAcceptanceRegistry;
    Record(Result,
        !MissingAcceptanceRegistry.LoadRegistry(
            FString(RegistryPath.string()), Failure) &&
        Failure == FString("sdr-v3-record-schema-or-identity"),
        "accepted SDR v3 baseline requires explicit maintainer acceptance");

    WriteText(RegistryPath, SdrBaselineV3Registry(
        SdrBaselineV3Json("accepted", Acceptance.c_str())));
    FOutputTransformSdrBaselineRegistryV3 AcceptedRegistry;
    Record(Result,
        AcceptedRegistry.LoadRegistry(FString(RegistryPath.string()), Failure) &&
        AcceptedRegistry.SelectAccepted(
            "production-content-lantern-v3", "metal",
            "macos.apple8.metal.sdr", "Sdr.sRGB.v1",
            "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            Selected, Failure) &&
        Selected.State == FString("accepted") &&
        Selected.Width == 512 && Selected.Height == 512 &&
        Selected.SampleCount == 1 && Selected.Acceptance.has_value() &&
        Selected.Acceptance->MaintainerId == FString("maintainer"),
        "SDR v3 selects only an exact fresh key with explicit acceptance");

    const struct
    {
        const char* Workload;
        const char* Backend;
        const char* Device;
        const char* Profile;
        const char* Transform;
        double Exposure;
        const char* Settings;
    } KeyMismatches[] = {
        {"production-content-sponza-v3", "metal", "macos.apple8.metal.sdr",
            "Sdr.sRGB.v1", "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        {"production-content-lantern-v3", "vulkan", "macos.apple8.metal.sdr",
            "Sdr.sRGB.v1", "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        {"production-content-lantern-v3", "metal", "another.device",
            "Sdr.sRGB.v1", "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        {"production-content-lantern-v3", "metal", "macos.apple8.metal.sdr",
            "Sdr.BT709.v1", "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        {"production-content-lantern-v3", "metal", "macos.apple8.metal.sdr",
            "Sdr.sRGB.v1", "Sdr.NarkowiczAcesFit.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        {"production-content-lantern-v3", "metal", "macos.apple8.metal.sdr",
            "Sdr.sRGB.v1", "Sdr.KhronosPbrNeutral.v1", 1.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        {"production-content-lantern-v3", "metal", "macos.apple8.metal.sdr",
            "Sdr.sRGB.v1", "Sdr.KhronosPbrNeutral.v1", 0.0,
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
    };
    bool bAllKeysFailClosed = true;
    for (const auto& Mismatch : KeyMismatches)
    {
        bAllKeysFailClosed = bAllKeysFailClosed &&
            !AcceptedRegistry.SelectAccepted(
                Mismatch.Workload, Mismatch.Backend, Mismatch.Device,
                Mismatch.Profile, Mismatch.Transform, Mismatch.Exposure,
                Mismatch.Settings, Selected, Failure) &&
            Failure == FString("sdr-v3-baseline-missing");
    }
    Record(Result, bAllKeysFailClosed,
        "SDR v3 authority never crosses workload, backend, device, profile, transform, exposure, or settings keys");

    std::string InvalidExtent = SdrBaselineV3Json(
        "accepted", Acceptance.c_str());
    const auto Width = InvalidExtent.find("\"width\":512");
    InvalidExtent.replace(Width, std::strlen("\"width\":512"),
        "\"width\":511");
    WriteText(RegistryPath, SdrBaselineV3Registry(InvalidExtent));
    FOutputTransformSdrBaselineRegistryV3 InvalidExtentRegistry;
    Record(Result,
        !InvalidExtentRegistry.LoadRegistry(
            FString(RegistryPath.string()), Failure) &&
        Failure == FString("sdr-v3-record-schema-or-identity"),
        "SDR v3 baseline rejects non-exact 512-by-512 authority");

    std::string WrongAcceptedDigest = SdrBaselineV3Json(
        "accepted", Acceptance.c_str());
    const auto CandidateDigest = WrongAcceptedDigest.find(
        "\"candidateSha256\":\"cccc");
    WrongAcceptedDigest[CandidateDigest +
        std::strlen("\"candidateSha256\":\"")] = 'f';
    WriteText(RegistryPath, SdrBaselineV3Registry(WrongAcceptedDigest));
    FOutputTransformSdrBaselineRegistryV3 WrongDigestRegistry;
    Record(Result,
        !WrongDigestRegistry.LoadRegistry(
            FString(RegistryPath.string()), Failure),
        "maintainer acceptance is bound to the exact compressed Candidate digest");

    WriteText(RegistryPath, SdrBaselineV3Registry(
        SdrBaselineV3Json("candidate", "null")));
    FOutputTransformSdrBaselineRegistryV3 V3OnlyRegistry;
    Record(Result,
        V3OnlyRegistry.LoadRegistry(FString(RegistryPath.string()), Failure) &&
        !V3OnlyRegistry.SelectAccepted(
            "production-content-lantern-v2", "metal",
            "macos.apple8.metal.sdr", "Sdr.sRGB.v1",
            "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            Selected, Failure) &&
        Failure == FString("sdr-v3-selection-contract"),
        "SDR v3 loader cannot reinterpret or carry forward Feature 028 v2 authority");

    FOutputTransformSdrBaselineRegistryV3 RepositoryRegistry;
    Record(Result,
        RepositoryRegistry.LoadRegistry(
            "Config/Validation/OutputTransform/SDR/Baselines-v3.json",
            Failure) &&
        !RepositoryRegistry.SelectAccepted(
            "production-content-lantern-v3", "metal",
            "macos.apple8.metal.sdr", "Sdr.sRGB.v1",
            "Sdr.KhronosPbrNeutral.v1", 0.0,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            Selected, Failure) &&
        Failure == FString("sdr-v3-baseline-missing"),
        "empty repository v3 registry fails closed until fresh maintainer acceptance");
    std::filesystem::remove_all(Root, Error);
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
        Failure == FString("baseline-state-not-accepted") &&
        Baseline.DeviceClass == FString("macos.apple8.metal.rgba8"),
        "baseline rejection retains the exact registry-derived device class");

    std::string InvalidExtent = BaselineJson("accepted", "lantern-metal");
    const auto Extent = InvalidExtent.find("\"width\":512");
    InvalidExtent.replace(Extent, std::strlen("\"width\":512"),
        "\"width\":256");
    WriteText(Root / "accepted.json", InvalidExtent);
    FProductionImageBaselineRegistry InvalidExtentRegistry;
    Record(Result,
        InvalidExtentRegistry.LoadDeviceClasses(
            FString(RegistryPath.string()), Failure) &&
        !InvalidExtentRegistry.LoadBaselines(FString(Root.string()), Failure) &&
        Failure == FString("baseline-schema-or-identity"),
        "baseline registry rejects non-canonical formal image extents");

    std::string RawReference = BaselineJson("accepted", "lantern-metal");
    const auto PngExtension = RawReference.find("lantern.png");
    RawReference.replace(PngExtension, std::strlen("lantern.png"),
        "lantern.ppm");
    WriteText(Root / "accepted.json", RawReference);
    FProductionImageBaselineRegistry RawReferenceRegistry;
    Record(Result,
        RawReferenceRegistry.LoadDeviceClasses(
            FString(RegistryPath.string()), Failure) &&
        !RawReferenceRegistry.LoadBaselines(FString(Root.string()), Failure) &&
        Failure == FString("baseline-schema-or-identity"),
        "baseline registry rejects uncompressed PPM references");

    const std::string Alpha = ReferenceJson(
        "alpha", "lantern-alpha.png", 'a', 'b');
    const std::string Beta = ReferenceJson(
        "beta", "lantern-beta.png", 'c', 'd');
    WriteText(Root / "accepted.json", BaselineJsonWithReferences(
        "accepted", "lantern-metal", Alpha + "," + Beta));
    FProductionImageBaselineRegistry MultiReferenceRegistry;
    Record(Result,
        MultiReferenceRegistry.LoadDeviceClasses(
            FString(RegistryPath.string()), Failure) &&
        MultiReferenceRegistry.LoadBaselines(
            FString(Root.string()), Failure) &&
        MultiReferenceRegistry.SelectAccepted(Signature,
            "production-lantern-v1", "metal", Baseline, Failure) &&
        Baseline.References.size() == 2 &&
        Baseline.References[0].ReferenceId == FString("alpha") &&
        Baseline.References[1].ReferenceId == FString("beta"),
        "baseline v2 accepts one canonical ordered two-reference set");

    WriteText(Root / "accepted.json", BaselineJsonWithReferences(
        "accepted", "lantern-metal", Beta + "," + Alpha));
    FProductionImageBaselineRegistry UnsortedRegistry;
    Record(Result,
        UnsortedRegistry.LoadDeviceClasses(
            FString(RegistryPath.string()), Failure) &&
        !UnsortedRegistry.LoadBaselines(FString(Root.string()), Failure) &&
        Failure == FString("baseline-schema-or-identity"),
        "baseline v2 rejects non-canonical reference ordering");

    WriteText(Root / "accepted.json", BaselineJsonWithReferences(
        "accepted", "lantern-metal", Alpha + "," + Alpha));
    FProductionImageBaselineRegistry DuplicateRegistry;
    Record(Result,
        DuplicateRegistry.LoadDeviceClasses(
            FString(RegistryPath.string()), Failure) &&
        !DuplicateRegistry.LoadBaselines(FString(Root.string()), Failure) &&
        Failure == FString("baseline-schema-or-identity"),
        "baseline v2 rejects duplicate references");

    const std::string Gamma = ReferenceJson(
        "gamma", "lantern-gamma.png", 'e', 'f');
    const std::string Omega = ReferenceJson(
        "omega", "lantern-omega.png", '1', '2');
    WriteText(Root / "accepted.json", BaselineJsonWithReferences(
        "accepted", "lantern-metal",
        Alpha + "," + Beta + "," + Gamma + "," + Omega));
    FProductionImageBaselineRegistry OversizedRegistry;
    Record(Result,
        OversizedRegistry.LoadDeviceClasses(
            FString(RegistryPath.string()), Failure) &&
        !OversizedRegistry.LoadBaselines(FString(Root.string()), Failure) &&
        Failure == FString("baseline-schema-or-identity"),
        "baseline v2 rejects more than three references");

    FProductionImageBaselineRegistry RepositoryRegistry;
    const FProductionCapabilitySignature MetalSignature{
        1, "native-metal", "arm64", "apple8", "metal-macos-12-arm64",
        "rgba8-unorm", "d32-float", 1, "astc"};
    const FProductionCapabilitySignature WindowsSignature{
        1, "native-vulkan", "x86_64", "discrete-vulkan", "vulkan-1.3",
        "rgba8-unorm", "d32-float", 1, "bc"};
    Record(Result,
        RepositoryRegistry.LoadDeviceClasses(
            "Config/Validation/ProductionContent/DeviceClasses.json",
            Failure) &&
        RepositoryRegistry.LoadBaselines(
            "Content/ProductionAcceptance/Baselines", Failure) &&
        RepositoryRegistry.SelectAccepted(MetalSignature,
            "production-content-lantern-v2", "metal", Baseline, Failure) &&
        Baseline.BaselineId == FString(
            "production-content-lantern-v2.macos.apple8.metal.rgba8.v2") &&
        Baseline.References.size() == 1 &&
        Baseline.References.front().ReferenceSha256 == FString(
            "0ac8a8f04d32ead184dd9f4b22dcf40d6932a50fdfe993f1bc39268ed773172f"),
        "repository registry consumes the explicitly accepted exact-drawable Metal Lantern baseline");
    Record(Result,
        RepositoryRegistry.SelectAccepted(MetalSignature,
            "production-content-sponza-v2", "metal", Baseline, Failure) &&
        Baseline.BaselineId == FString(
            "production-content-sponza-v2.macos.apple8.metal.rgba8.v2") &&
        Baseline.References.size() == 1 &&
        Baseline.References.front().ReferenceSha256 == FString(
            "b2c7f49b45fb3c695229c66a1f29e93a5b41bdb9b69cca0499e2b278c313cb93"),
        "repository registry consumes the explicitly accepted exact-drawable Metal Sponza baseline");
    Record(Result,
        RepositoryRegistry.SelectAccepted(WindowsSignature,
            "production-content-lantern-v2", "vulkan", Baseline, Failure) &&
        Baseline.BaselineId == FString(
            "production-content-lantern-v2.windows.discrete-vulkan.rgba8.v1") &&
        Baseline.References.size() == 1 &&
        Baseline.References.front().ReferencePath == FString(
            "windows.discrete-vulkan.rgba8/production-content-lantern-v2.png"),
        "repository registry consumes the explicitly accepted Windows PNG baseline");
    Record(Result,
        RepositoryRegistry.SelectAccepted(WindowsSignature,
            "production-content-sponza-v2", "vulkan", Baseline, Failure) &&
        Baseline.BaselineId == FString(
            "production-content-sponza-v2.windows.discrete-vulkan.rgba8.v1") &&
        Baseline.References.size() == 1 &&
        Baseline.References.front().ReferencePath == FString(
            "windows.discrete-vulkan.rgba8/production-content-sponza-v2.png"),
        "repository registry consumes the explicitly accepted Windows Sponza PNG baseline");
    std::filesystem::remove_all(Root, Error);
}

} // namespace

FProductionImageAcceptanceTestResult RunProductionImageAcceptanceTests()
{
    FProductionImageAcceptanceTestResult Result;
    TestReadbackNormalization(Result);
    TestSemanticProbeOrdering(Result);
    TestReadbackRegionStatistics(Result);
    TestFlipAndMutation(Result);
    TestNativeEvidence(Result);
    TestAuthoritativeFrameBundle(Result);
    TestReferenceSetSelection(Result);
    TestWorkloadRegions(Result);
    TestAcceptedReferenceRegionCalibration(Result);
    TestBaselineRegistry(Result);
    TestSdrBaselineV3Registry(Result);
    return Result;
}
