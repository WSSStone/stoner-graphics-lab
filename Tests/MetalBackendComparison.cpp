#include "MetalBackendComparison.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{

using Stoner::Core::uint8;
using Stoner::Core::uint16;
using Stoner::Core::uint32;

using FPixel = std::array<double, 4>;

struct FCanonicalReadback
{
    Stoner::Core::TArray<FPixel> Pixels;
};

FMetalBackendComparisonReport Fail(const char* Code, const char* Reason)
{
    FMetalBackendComparisonReport Report;
    Report.FailureCode = Code;
    Report.FailureReason = Reason;
    return Report;
}

uint32 BytesPerPixel(EMetalReadbackFormat Format) noexcept
{
    switch (Format)
    {
    case EMetalReadbackFormat::RGBA8UNorm: return 4;
    case EMetalReadbackFormat::RGBA16Float: return 8;
    case EMetalReadbackFormat::R32Float: return 4;
    }
    return 0;
}

double HalfToDouble(uint16 Value) noexcept
{
    const uint32 Sign = static_cast<uint32>(Value >> 15);
    const uint32 Exponent = static_cast<uint32>((Value >> 10) & 0x1f);
    const uint32 Fraction = static_cast<uint32>(Value & 0x3ff);
    double Magnitude = 0.0;
    if (Exponent == 0)
        Magnitude = std::ldexp(static_cast<double>(Fraction), -24);
    else if (Exponent == 31)
        Magnitude = Fraction == 0
            ? std::numeric_limits<double>::infinity()
            : std::numeric_limits<double>::quiet_NaN();
    else
        Magnitude = std::ldexp(
            1.0 + static_cast<double>(Fraction) / 1024.0,
            static_cast<int>(Exponent) - 15);
    return Sign ? -Magnitude : Magnitude;
}

double DecodeSrgb(double Value) noexcept
{
    return Value <= 0.04045
        ? Value / 12.92
        : std::pow((Value + 0.055) / 1.055, 2.4);
}

bool Canonicalize(
    const FMetalBackendReadback& Input,
    FCanonicalReadback& Out,
    FMetalBackendComparisonReport& Error)
{
    Out = {};
    const uint32 PixelBytes = BytesPerPixel(Input.Format);
    const Stoner::Core::uint64 LogicalRowBytes =
        static_cast<Stoner::Core::uint64>(Input.Width) * PixelBytes;
    const Stoner::Core::uint64 ExpectedBytes =
        static_cast<Stoner::Core::uint64>(Input.RowPitchBytes) * Input.Height;
    if (Input.Backend.IsEmpty() || Input.EvidenceReference.IsEmpty() ||
        Input.WorkloadIdentity.IsEmpty() || Input.ShaderVersion.IsEmpty() ||
        Input.Width == 0 || Input.Height == 0 || PixelBytes == 0 ||
        Input.RowPitchBytes < LogicalRowBytes || Input.Bytes.size() != ExpectedBytes)
    {
        Error = Fail("METAL-COMPARE-INPUT", "readback metadata and byte extent must be complete");
        return false;
    }
    if (Input.ScalarChannel >= 4)
    {
        Error = Fail("METAL-COMPARE-CHANNEL", "scalar channel is outside the decoded pixel");
        return false;
    }

    Out.Pixels.reserve(static_cast<std::size_t>(Input.Width) * Input.Height);
    for (uint32 Y = 0; Y < Input.Height; ++Y)
    {
        const uint32 SourceY = Input.Origin == EMetalReadbackOrigin::TopLeft
            ? Y : Input.Height - 1 - Y;
        const uint8* Row = Input.Bytes.data() +
            static_cast<std::size_t>(SourceY) * Input.RowPitchBytes;
        for (uint32 X = 0; X < Input.Width; ++X)
        {
            const uint8* Source = Row + static_cast<std::size_t>(X) * PixelBytes;
            FPixel Pixel = {0.0, 0.0, 0.0, 1.0};
            if (Input.Format == EMetalReadbackFormat::RGBA8UNorm)
            {
                for (uint32 Channel = 0; Channel < 4; ++Channel)
                    Pixel[Channel] = static_cast<double>(Source[Channel]) / 255.0;
            }
            else if (Input.Format == EMetalReadbackFormat::RGBA16Float)
            {
                for (uint32 Channel = 0; Channel < 4; ++Channel)
                {
                    uint16 Bits = 0;
                    std::memcpy(&Bits, Source + Channel * sizeof(uint16), sizeof(Bits));
                    Pixel[Channel] = HalfToDouble(Bits);
                }
            }
            else
            {
                float Value = 0.0f;
                std::memcpy(&Value, Source, sizeof(Value));
                Pixel[0] = Value;
            }

            if (Input.Semantic == EMetalReadbackSemantic::FinalLdrColor &&
                Input.ColorSpace == EMetalReadbackColorSpace::SRGB)
            {
                for (uint32 Channel = 0; Channel < 3; ++Channel)
                    Pixel[Channel] = DecodeSrgb(Pixel[Channel]);
            }
            if (Input.Semantic == EMetalReadbackSemantic::NormalizedDepth &&
                Input.DepthConvention == EMetalReadbackDepthConvention::ReversedZ)
                Pixel[0] = 1.0 - Pixel[0];
            if (Input.Semantic == EMetalReadbackSemantic::WorldNormal &&
                Input.bNormalEncodedUNorm)
            {
                for (uint32 Channel = 0; Channel < 3; ++Channel)
                    Pixel[Channel] = Pixel[Channel] * 2.0 - 1.0;
            }
            if (!std::all_of(Pixel.begin(), Pixel.end(),
                    [](double Value) { return std::isfinite(Value); }))
            {
                Error = Fail("METAL-COMPARE-NONFINITE", "decoded readback contains a non-finite value");
                return false;
            }
            Out.Pixels.push_back(Pixel);
        }
    }
    return true;
}

bool Normalize3(FPixel& Pixel) noexcept
{
    const double Length = std::sqrt(
        Pixel[0] * Pixel[0] + Pixel[1] * Pixel[1] + Pixel[2] * Pixel[2]);
    if (!std::isfinite(Length) || Length <= 1e-12) return false;
    Pixel[0] /= Length;
    Pixel[1] /= Length;
    Pixel[2] /= Length;
    return true;
}

} // namespace

FMetalBackendComparisonReport CompareMetalBackendReadbacks(
    const FMetalBackendReadback& Left,
    const FMetalBackendReadback& Right)
{
    if (Left.Backend == Right.Backend ||
        Left.EvidenceReference == Right.EvidenceReference)
        return Fail("METAL-COMPARE-EVIDENCE", "comparison requires distinct backends and native evidence");
    if (Left.WorkloadIdentity != Right.WorkloadIdentity ||
        Left.ShaderVersion != Right.ShaderVersion)
        return Fail("METAL-COMPARE-IDENTITY", "workload and Shader Asset versions must match exactly");
    if (Left.Width != Right.Width || Left.Height != Right.Height ||
        Left.Semantic != Right.Semantic || Left.bWholeImage != Right.bWholeImage ||
        Left.ScalarChannel != Right.ScalarChannel)
        return Fail("METAL-COMPARE-SHAPE", "extent semantic and comparison mode must match");

    FMetalBackendComparisonReport Report;
    Report.LeftBackend = Left.Backend;
    Report.RightBackend = Right.Backend;
    Report.LeftEvidenceReference = Left.EvidenceReference;
    Report.RightEvidenceReference = Right.EvidenceReference;
    Report.WorkloadIdentity = Left.WorkloadIdentity;
    Report.ShaderVersion = Left.ShaderVersion;
    FCanonicalReadback CanonicalLeft;
    FCanonicalReadback CanonicalRight;
    if (!Canonicalize(Left, CanonicalLeft, Report) ||
        !Canonicalize(Right, CanonicalRight, Report))
        return Report;

    Report.ComparedPixelCount = CanonicalLeft.Pixels.size();
    Report.MinimumNormalDot = 1.0;
    bool bEverySemanticSamplePassed = true;
    for (std::size_t Index = 0; Index < CanonicalLeft.Pixels.size(); ++Index)
    {
        FPixel A = CanonicalLeft.Pixels[Index];
        FPixel B = CanonicalRight.Pixels[Index];
        bool bPrimaryPass = true;
        bool bMaximumPass = true;
        if (Left.Semantic == EMetalReadbackSemantic::WorldNormal)
        {
            if (!Normalize3(A) || !Normalize3(B))
                return Fail("METAL-COMPARE-NORMAL", "world normal cannot be normalized");
            const double Dot = std::clamp(
                A[0] * B[0] + A[1] * B[1] + A[2] * B[2], -1.0, 1.0);
            Report.MinimumNormalDot = std::min(Report.MinimumNormalDot, Dot);
            bPrimaryPass = Dot >= 0.999;
            Report.MaximumAbsoluteError = std::max(
                Report.MaximumAbsoluteError, 1.0 - Dot);
        }
        else
        {
            uint32 FirstChannel = 0;
            uint32 ChannelCount = 1;
            if (Left.Semantic == EMetalReadbackSemantic::FinalLdrColor ||
                Left.Semantic == EMetalReadbackSemantic::LinearHdrColor)
                ChannelCount = 4;
            else if (Left.Semantic == EMetalReadbackSemantic::Metallic ||
                Left.Semantic == EMetalReadbackSemantic::Roughness ||
                Left.Semantic == EMetalReadbackSemantic::AmbientOcclusion ||
                Left.Semantic == EMetalReadbackSemantic::ScalarLighting)
                FirstChannel = Left.ScalarChannel;

            for (uint32 Offset = 0; Offset < ChannelCount; ++Offset)
            {
                const uint32 Channel = FirstChannel + Offset;
                const double Error = std::abs(A[Channel] - B[Channel]);
                Report.MaximumAbsoluteError = std::max(
                    Report.MaximumAbsoluteError, Error);
                double PrimaryTolerance = 1e-3;
                double MaximumTolerance = PrimaryTolerance;
                if (Left.Semantic == EMetalReadbackSemantic::FinalLdrColor)
                {
                    PrimaryTolerance = 2.0 / 255.0;
                    MaximumTolerance = Left.bWholeImage ? 4.0 / 255.0 : PrimaryTolerance;
                }
                else if (Left.Semantic == EMetalReadbackSemantic::LinearHdrColor ||
                    Left.Semantic == EMetalReadbackSemantic::ScalarLighting)
                    PrimaryTolerance = MaximumTolerance =
                        std::max(1e-3, 1e-3 * std::abs(A[Channel]));
                else if (Left.Semantic == EMetalReadbackSemantic::NormalizedDepth)
                    PrimaryTolerance = MaximumTolerance = 1e-4;
                else if (Left.Semantic == EMetalReadbackSemantic::AmbientOcclusion)
                    PrimaryTolerance = MaximumTolerance = 2e-3;
                bPrimaryPass = bPrimaryPass && Error <= PrimaryTolerance;
                bMaximumPass = bMaximumPass && Error <= MaximumTolerance;
            }
        }
        if (bPrimaryPass) ++Report.WithinPrimaryTolerancePixelCount;
        bEverySemanticSamplePassed = bEverySemanticSamplePassed &&
            bPrimaryPass && bMaximumPass;
    }

    Report.WithinPrimaryToleranceRatio = Report.ComparedPixelCount == 0
        ? 0.0
        : static_cast<double>(Report.WithinPrimaryTolerancePixelCount) /
            static_cast<double>(Report.ComparedPixelCount);
    Report.bPassed = Left.bWholeImage
        ? bEverySemanticSamplePassed ||
            (Report.WithinPrimaryToleranceRatio >= 0.995 &&
                Report.MaximumAbsoluteError <= 4.0 / 255.0)
        : bEverySemanticSamplePassed;
    if (!Report.bPassed)
    {
        Report.FailureCode = "METAL-COMPARE-TOLERANCE";
        Report.FailureReason = "normalized samples exceed the frozen tolerance set";
    }
    return Report;
}

Stoner::Core::FString FMetalBackendComparisonReport::Dump() const
{
    std::ostringstream Stream;
    Stream << "tolerance-set=" << ToleranceSet << '\n'
        << "result=" << (bPassed ? "pass" : "fail") << '\n'
        << "left-backend=" << LeftBackend.CStr() << '\n'
        << "right-backend=" << RightBackend.CStr() << '\n'
        << "left-evidence=" << LeftEvidenceReference.CStr() << '\n'
        << "right-evidence=" << RightEvidenceReference.CStr() << '\n'
        << "workload=" << WorkloadIdentity.CStr() << '\n'
        << "shader-version=" << ShaderVersion.CStr() << '\n'
        << "compared-pixels=" << ComparedPixelCount << '\n'
        << "within-primary-pixels=" << WithinPrimaryTolerancePixelCount << '\n'
        << std::fixed << std::setprecision(9)
        << "within-primary-ratio=" << WithinPrimaryToleranceRatio << '\n'
        << "maximum-absolute-error=" << MaximumAbsoluteError << '\n'
        << "minimum-normal-dot=" << MinimumNormalDot << '\n'
        << "failure-code=" << FailureCode.CStr() << '\n'
        << "failure-reason=" << FailureReason.CStr() << '\n';
    return Stoner::Core::FString(Stream.str());
}
