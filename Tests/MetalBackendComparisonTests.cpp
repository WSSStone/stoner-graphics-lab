#include "MetalBackendComparisonTests.h"

#include "MetalBackendComparison.h"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{

using Stoner::Core::uint8;

void Record(
    FMetalBackendComparisonTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FMetalBackendReadback Base(
    const char* Backend,
    const char* Evidence,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height)
{
    FMetalBackendReadback Value;
    Value.Backend = Backend;
    Value.EvidenceReference = Evidence;
    Value.WorkloadIdentity = "scene/deferred/reference-v1";
    Value.ShaderVersion = "sha256:027";
    Value.Width = Width;
    Value.Height = Height;
    Value.RowPitchBytes = Width * 4;
    return Value;
}

FMetalBackendReadback Ldr(
    const char* Backend,
    const char* Evidence,
    Stoner::Core::TArray<uint8> Bytes)
{
    auto Value = Base(Backend, Evidence,
        static_cast<Stoner::Core::uint32>(Bytes.size() / 4), 1);
    Value.Bytes = std::move(Bytes);
    return Value;
}

Stoner::Core::TArray<uint8> FloatBytes(float Value)
{
    Stoner::Core::TArray<uint8> Bytes(sizeof(Value));
    std::memcpy(Bytes.data(), &Value, sizeof(Value));
    return Bytes;
}

} // namespace

FMetalBackendComparisonTestResult RunMetalBackendComparisonTests()
{
    FMetalBackendComparisonTestResult Result;

    auto Top = Base("metal", "metal/orientation", 2, 2);
    Top.Bytes = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255};
    auto BottomPadded = Base("vulkan", "vulkan/orientation", 2, 2);
    BottomPadded.Origin = EMetalReadbackOrigin::BottomLeft;
    BottomPadded.RowPitchBytes = 12;
    BottomPadded.Bytes = {
        0, 0, 255, 255, 255, 255, 255, 255, 99, 99, 99, 99,
        255, 0, 0, 255, 0, 255, 0, 255, 88, 88, 88, 88};
    Record(Result, CompareMetalBackendReadbacks(Top, BottomPadded).bPassed,
        "comparison normalizes origin and strips backend row padding");

    auto Srgb = Ldr("metal", "metal/colorspace", {188, 188, 188, 255});
    Srgb.ColorSpace = EMetalReadbackColorSpace::SRGB;
    auto Linear = Ldr("vulkan", "vulkan/colorspace", {128, 128, 128, 255});
    Record(Result, CompareMetalBackendReadbacks(Srgb, Linear).bPassed,
        "comparison decodes declared sRGB into canonical linear color");

    auto StandardDepth = Base("metal", "metal/depth", 1, 1);
    StandardDepth.Format = EMetalReadbackFormat::R32Float;
    StandardDepth.Semantic = EMetalReadbackSemantic::NormalizedDepth;
    StandardDepth.Bytes = FloatBytes(0.25f);
    auto ReversedDepth = StandardDepth;
    ReversedDepth.Backend = "vulkan";
    ReversedDepth.EvidenceReference = "vulkan/depth";
    ReversedDepth.DepthConvention = EMetalReadbackDepthConvention::ReversedZ;
    ReversedDepth.Bytes = FloatBytes(0.75f);
    Record(Result, CompareMetalBackendReadbacks(StandardDepth, ReversedDepth).bPassed,
        "comparison normalizes StandardZ and ReversedZ depth");

    auto DirectNormal = Base("metal", "metal/normal", 1, 1);
    DirectNormal.Format = EMetalReadbackFormat::RGBA16Float;
    DirectNormal.Semantic = EMetalReadbackSemantic::WorldNormal;
    DirectNormal.RowPitchBytes = 8;
    DirectNormal.Bytes = {0, 0, 0, 0, 0, 60, 0, 60};
    auto EncodedNormal = Base("vulkan", "vulkan/normal", 1, 1);
    EncodedNormal.Semantic = EMetalReadbackSemantic::WorldNormal;
    EncodedNormal.bNormalEncodedUNorm = true;
    EncodedNormal.Bytes = {128, 128, 255, 255};
    const auto NormalReport = CompareMetalBackendReadbacks(
        DirectNormal, EncodedNormal);
    Record(Result, NormalReport.bPassed && NormalReport.MinimumNormalDot >= 0.999,
        "comparison decodes and normalizes world-space normals");

    auto ExactLeft = Ldr("metal", "metal/exact", {10, 10, 10, 255});
    auto ExactRight = Ldr("vulkan", "vulkan/exact", {12, 12, 12, 255});
    auto OverRight = ExactRight;
    OverRight.EvidenceReference = "vulkan/over";
    OverRight.Bytes = {13, 13, 13, 255};
    Record(Result,
        CompareMetalBackendReadbacks(ExactLeft, ExactRight).bPassed &&
            !CompareMetalBackendReadbacks(ExactLeft, OverRight).bPassed,
        "comparison accepts exact semantic threshold and rejects the next LDR step");

    Stoner::Core::TArray<uint8> Reference(200 * 4, 0);
    for (std::size_t Index = 3; Index < Reference.size(); Index += 4)
        Reference[Index] = 255;
    auto WholeLeft = Ldr("metal", "metal/whole", Reference);
    WholeLeft.bWholeImage = true;
    auto WholeOneOutlier = Ldr("vulkan", "vulkan/whole-one", Reference);
    WholeOneOutlier.bWholeImage = true;
    WholeOneOutlier.Bytes[0] = 3;
    auto WholeTwoOutliers = WholeOneOutlier;
    WholeTwoOutliers.EvidenceReference = "vulkan/whole-two";
    WholeTwoOutliers.Bytes[4] = 3;
    auto WholeExcessMaximum = WholeOneOutlier;
    WholeExcessMaximum.EvidenceReference = "vulkan/whole-max";
    WholeExcessMaximum.Bytes[0] = 5;
    const auto WholeReport = CompareMetalBackendReadbacks(
        WholeLeft, WholeOneOutlier);
    Record(Result,
        WholeReport.bPassed && WholeReport.WithinPrimaryToleranceRatio == 0.995 &&
            !CompareMetalBackendReadbacks(WholeLeft, WholeTwoOutliers).bPassed &&
            !CompareMetalBackendReadbacks(WholeLeft, WholeExcessMaximum).bPassed,
        "whole-image comparison enforces 99.5 percent and four-step maximum error");

    auto ReusedEvidence = ExactRight;
    ReusedEvidence.EvidenceReference = ExactLeft.EvidenceReference;
    auto WrongIdentity = ExactRight;
    WrongIdentity.WorkloadIdentity = "different/workload";
    auto WrongChannel = ExactRight;
    WrongChannel.ScalarChannel = 1;
    Record(Result,
        !CompareMetalBackendReadbacks(ExactLeft, ReusedEvidence).bPassed &&
            !CompareMetalBackendReadbacks(ExactLeft, WrongIdentity).bPassed &&
            !CompareMetalBackendReadbacks(ExactLeft, WrongChannel).bPassed,
        "comparison rejects reused evidence and unmatched identity metadata");

    auto NonFinite = StandardDepth;
    NonFinite.EvidenceReference = "metal/nonfinite";
    NonFinite.Bytes = FloatBytes(
        std::numeric_limits<float>::quiet_NaN());
    Record(Result,
        !CompareMetalBackendReadbacks(NonFinite, ReversedDepth).bPassed,
        "comparison rejects non-finite decoded readback values");

    const auto StableReport = CompareMetalBackendReadbacks(Top, BottomPadded);
    Record(Result,
        StableReport.Dump().View().find("metal-vulkan-tolerance-v1") !=
            std::string_view::npos &&
        StableReport.Dump().View().find("left-evidence=metal/orientation") !=
            std::string_view::npos &&
        StableReport.Dump().View().find("right-evidence=vulkan/orientation") !=
            std::string_view::npos &&
        StableReport.Dump() == CompareMetalBackendReadbacks(Top, BottomPadded).Dump(),
        "comparison report records frozen tolerance and is byte-stable");
    return Result;
}
