#include "ProductionImageAcceptance.h"

#include "FLIP.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

namespace
{
using namespace Stoner::Core;

float SrgbToLinear(float Value)
{
    return Value <= 0.04045f
        ? Value / 12.92f
        : std::pow((Value + 0.055f) / 1.055f, 2.4f);
}

float HalfToFloat(uint16 Value)
{
    const uint32 Sign = static_cast<uint32>(Value & 0x8000u) << 16u;
    uint32 Exponent = (Value >> 10u) & 0x1fu;
    uint32 Mantissa = Value & 0x03ffu;
    uint32 Bits = 0;
    if (Exponent == 0)
    {
        if (Mantissa == 0)
        {
            Bits = Sign;
        }
        else
        {
            Exponent = 127u - 15u + 1u;
            while ((Mantissa & 0x0400u) == 0)
            {
                Mantissa <<= 1u;
                --Exponent;
            }
            Bits = Sign | (Exponent << 23u) |
                ((Mantissa & 0x03ffu) << 13u);
        }
    }
    else if (Exponent == 0x1fu)
    {
        Bits = Sign | 0x7f800000u | (Mantissa << 13u);
    }
    else
    {
        Bits = Sign | ((Exponent + 127u - 15u) << 23u) |
            (Mantissa << 13u);
    }
    float Result = 0.0f;
    std::memcpy(&Result, &Bits, sizeof(Result));
    return Result;
}

uint32 BytesPerPixel(EProductionReadbackPixelFormat Format)
{
    switch (Format)
    {
    case EProductionReadbackPixelFormat::RGBA8UNorm:
    case EProductionReadbackPixelFormat::BGRA8UNorm:
        return 4;
    case EProductionReadbackPixelFormat::RGBA16Float:
        return 8;
    case EProductionReadbackPixelFormat::RGBA32Float:
        return 16;
    case EProductionReadbackPixelFormat::R32Float:
        return 4;
    }
    return 0;
}

bool ReadPixel(const uint8* Bytes, EProductionReadbackPixelFormat Format,
    float& R, float& G, float& B)
{
    switch (Format)
    {
    case EProductionReadbackPixelFormat::RGBA8UNorm:
        R = Bytes[0] / 255.0f;
        G = Bytes[1] / 255.0f;
        B = Bytes[2] / 255.0f;
        return true;
    case EProductionReadbackPixelFormat::BGRA8UNorm:
        R = Bytes[2] / 255.0f;
        G = Bytes[1] / 255.0f;
        B = Bytes[0] / 255.0f;
        return true;
    case EProductionReadbackPixelFormat::RGBA16Float:
    {
        uint16 Channels[3]{};
        std::memcpy(Channels, Bytes, sizeof(Channels));
        R = HalfToFloat(Channels[0]);
        G = HalfToFloat(Channels[1]);
        B = HalfToFloat(Channels[2]);
        return true;
    }
    case EProductionReadbackPixelFormat::RGBA32Float:
    {
        float Channels[3]{};
        std::memcpy(Channels, Bytes, sizeof(Channels));
        R = Channels[0];
        G = Channels[1];
        B = Channels[2];
        return true;
    }
    case EProductionReadbackPixelFormat::R32Float:
        std::memcpy(&R, Bytes, sizeof(R));
        G = R;
        B = R;
        return true;
    }
    return false;
}

bool IsUnit(float Value)
{
    return std::isfinite(Value) && Value >= 0.0f && Value <= 1.0f;
}

} // namespace

bool FProductionCanonicalImage::IsValid() const noexcept
{
    if (Width == 0 || Height == 0 ||
        LinearRgb.size() != static_cast<Stoner::Core::usize>(Width) * Height * 3u)
        return false;
    return std::all_of(LinearRgb.begin(), LinearRgb.end(), IsUnit);
}

bool NormalizeProductionReadback(
    const FProductionReadbackView& Source,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutImage = {};
    OutFailure = {};
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if (Source.Width == 0 || Source.Height == 0 || PixelBytes == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size())
    {
        OutFailure = "invalid-readback-layout";
        return false;
    }

    FProductionCanonicalImage Candidate;
    Candidate.Width = Source.Width;
    Candidate.Height = Source.Height;
    try
    {
        Candidate.LinearRgb.resize(
            static_cast<usize>(Source.Width) * Source.Height * 3u);
    }
    catch (const std::bad_alloc&)
    {
        OutFailure = "image-allocation";
        return false;
    }

    for (uint32 Y = 0; Y < Source.Height; ++Y)
    {
        const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
            ? Y : Source.Height - 1u - Y;
        const uint8* Row = Source.Bytes.data() +
            static_cast<usize>(SourceY) * Source.RowPitchBytes;
        for (uint32 X = 0; X < Source.Width; ++X)
        {
            float R = 0.0f;
            float G = 0.0f;
            float B = 0.0f;
            if (!ReadPixel(Row + static_cast<usize>(X) * PixelBytes,
                    Source.Format, R, G, B) ||
                !std::isfinite(R) || !std::isfinite(G) || !std::isfinite(B) ||
                R < 0.0f || G < 0.0f || B < 0.0f ||
                R > 1.0f || G > 1.0f || B > 1.0f)
            {
                OutFailure = "non-finite-or-out-of-range-pixel";
                return false;
            }
            if (Source.Transfer == EProductionColorTransfer::SRGB)
            {
                R = SrgbToLinear(R);
                G = SrgbToLinear(G);
                B = SrgbToLinear(B);
            }
            const usize Destination =
                (static_cast<usize>(Y) * Source.Width + X) * 3u;
            Candidate.LinearRgb[Destination] = R;
            Candidate.LinearRgb[Destination + 1u] = G;
            Candidate.LinearRgb[Destination + 2u] = B;
        }
    }
    OutImage = std::move(Candidate);
    return true;
}

bool NormalizeProductionSignedNormalReadback(
    const FProductionReadbackView& Source,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutImage = {};
    OutFailure = {};
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if ((Source.Format != EProductionReadbackPixelFormat::RGBA16Float &&
         Source.Format != EProductionReadbackPixelFormat::RGBA32Float) ||
        Source.Transfer != EProductionColorTransfer::Linear ||
        Source.Width == 0 || Source.Height == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size())
    {
        OutFailure = "invalid-normal-readback-layout";
        return false;
    }
    FProductionCanonicalImage Candidate;
    Candidate.Width = Source.Width;
    Candidate.Height = Source.Height;
    try
    {
        Candidate.LinearRgb.resize(
            static_cast<usize>(Source.Width) * Source.Height * 3u);
    }
    catch (const std::bad_alloc&)
    {
        OutFailure = "image-allocation";
        return false;
    }
    for (uint32 Y = 0; Y < Source.Height; ++Y)
    {
        const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
            ? Y : Source.Height - 1u - Y;
        const uint8* Row = Source.Bytes.data() +
            static_cast<usize>(SourceY) * Source.RowPitchBytes;
        for (uint32 X = 0; X < Source.Width; ++X)
        {
            float NormalX = 0.0f;
            float NormalY = 0.0f;
            float NormalZ = 0.0f;
            if (!ReadPixel(Row + static_cast<usize>(X) * PixelBytes,
                    Source.Format, NormalX, NormalY, NormalZ) ||
                !std::isfinite(NormalX) || !std::isfinite(NormalY) ||
                !std::isfinite(NormalZ) || NormalX < -1.0f ||
                NormalX > 1.0f || NormalY < -1.0f || NormalY > 1.0f ||
                NormalZ < -1.0f || NormalZ > 1.0f)
            {
                OutFailure = "invalid-signed-normal";
                return false;
            }
            const usize Destination =
                (static_cast<usize>(Y) * Source.Width + X) * 3u;
            Candidate.LinearRgb[Destination] = NormalX * 0.5f + 0.5f;
            Candidate.LinearRgb[Destination + 1u] = NormalY * 0.5f + 0.5f;
            Candidate.LinearRgb[Destination + 2u] = NormalZ * 0.5f + 0.5f;
        }
    }
    OutImage = std::move(Candidate);
    return true;
}

bool SampleProductionReadbackPixel(
    const FProductionReadbackView& Source,
    Stoner::Core::uint32 X,
    Stoner::Core::uint32 Y,
    Stoner::Core::FVector3& OutValue,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutValue = {};
    OutFailure = {};
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if (Source.Width == 0 || Source.Height == 0 || X >= Source.Width ||
        Y >= Source.Height || PixelBytes == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size())
    {
        OutFailure = "sample-readback-layout";
        return false;
    }
    const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
        ? Y : Source.Height - 1u - Y;
    const uint8* Pixel = Source.Bytes.data() +
        static_cast<usize>(SourceY) * Source.RowPitchBytes +
        static_cast<usize>(X) * PixelBytes;
    if (!ReadPixel(Pixel, Source.Format, OutValue.X, OutValue.Y, OutValue.Z) ||
        !std::isfinite(OutValue.X) || !std::isfinite(OutValue.Y) ||
        !std::isfinite(OutValue.Z))
    {
        OutFailure = "sample-non-finite";
        return false;
    }
    if (Source.Transfer == EProductionColorTransfer::SRGB)
    {
        if (!IsUnit(OutValue.X) || !IsUnit(OutValue.Y) || !IsUnit(OutValue.Z))
        {
            OutFailure = "sample-srgb-range";
            return false;
        }
        OutValue.X = SrgbToLinear(OutValue.X);
        OutValue.Y = SrgbToLinear(OutValue.Y);
        OutValue.Z = SrgbToLinear(OutValue.Z);
    }
    return true;
}

FProductionSemanticProbeResult RunProductionSemanticProbes(
    const FProductionSemanticProbeRequest& Request)
{
    FProductionSemanticProbeResult Result;
    const auto Fail = [&Result](const Stoner::Core::FString& Reason) {
        Result.FirstFailure = Reason;
        return Result;
    };
    if (!Request.Color || !Request.Color->IsValid())
        return Fail("color-image");
    ++Result.PassedProbeCount;
    if (Request.ExpectedFrameToken == 0 ||
        Request.ExpectedFrameToken != Request.ObservedFrameToken)
        return Fail("current-frame");
    ++Result.PassedProbeCount;

    const auto& Pixels = Request.Color->LinearRgb;
    Stoner::Core::usize Covered = 0;
    float Minimum = 1.0f;
    float Maximum = 0.0f;
    for (Stoner::Core::usize Index = 0; Index < Pixels.size(); Index += 3u)
    {
        const float Luminance = 0.2126f * Pixels[Index] +
            0.7152f * Pixels[Index + 1u] + 0.0722f * Pixels[Index + 2u];
        Minimum = std::min(Minimum, Luminance);
        Maximum = std::max(Maximum, Luminance);
        if (Luminance > 1.0f / 255.0f) ++Covered;
    }
    if (Maximum - Minimum <= 1.0f / 255.0f)
        return Fail("nonblank");
    ++Result.PassedProbeCount;
    const float Coverage = static_cast<float>(Covered) /
        static_cast<float>(Request.Color->Width * Request.Color->Height);
    if (!std::isfinite(Coverage) ||
        Coverage < Request.MinimumCoverageFraction ||
        Coverage > Request.MaximumCoverageFraction)
        return Fail("coverage");
    ++Result.PassedProbeCount;

    for (const auto& Probe : Request.Regions)
    {
        if (Probe.Name.IsEmpty() || Probe.X >= Request.Color->Width ||
            Probe.Y >= Request.Color->Height || Probe.Tolerance < 0.0f)
            return Fail("region-contract");
        const Stoner::Core::usize Pixel =
            (static_cast<Stoner::Core::usize>(Probe.Y) * Request.Color->Width +
             Probe.X) * 3u;
        if (std::abs(Pixels[Pixel] - Probe.Expected.X) > Probe.Tolerance ||
            std::abs(Pixels[Pixel + 1u] - Probe.Expected.Y) > Probe.Tolerance ||
            std::abs(Pixels[Pixel + 2u] - Probe.Expected.Z) > Probe.Tolerance)
            return Fail(Stoner::Core::FString(
                std::string("region-") + Probe.Name.ToStdString()));
        ++Result.PassedProbeCount;
    }
    static const Stoner::Core::TArray<Stoner::Core::FString> MandatoryRegions = {
        "orientation", "primitive-material", "base-color", "normal-response",
        "metallic-roughness", "emissive"};
    Stoner::Core::TArray<Stoner::Core::FString> RequiredRegions = MandatoryRegions;
    RequiredRegions.insert(RequiredRegions.end(),
        Request.RequiredRegionNames.begin(), Request.RequiredRegionNames.end());
    for (const auto& Required : RequiredRegions)
    {
        const auto Found = std::find_if(
            Request.Regions.begin(), Request.Regions.end(),
            [&Required](const FProductionRegionProbe& Probe) {
                return Probe.Name == Required;
            });
        if (Required.IsEmpty() || Found == Request.Regions.end())
            return Fail(Stoner::Core::FString(
                std::string("missing-region-") + Required.ToStdString()));
        ++Result.PassedProbeCount;
    }
    if (!Request.Normal)
        return Fail("normal-attachment");
    if (Request.Normal)
    {
        if (!Request.Normal->IsValid() ||
            Request.Normal->Width != Request.Color->Width ||
            Request.Normal->Height != Request.Color->Height)
            return Fail("normal-attachment");
        bool bObservedUnitNormal = false;
        for (Stoner::Core::usize Index = 0;
             Index < Request.Normal->LinearRgb.size(); Index += 3u)
        {
            const float X = Request.Normal->LinearRgb[Index] * 2.0f - 1.0f;
            const float Y = Request.Normal->LinearRgb[Index + 1u] * 2.0f - 1.0f;
            const float Z = Request.Normal->LinearRgb[Index + 2u] * 2.0f - 1.0f;
            const float Length = std::sqrt(X * X + Y * Y + Z * Z);
            bObservedUnitNormal = bObservedUnitNormal ||
                std::abs(Length - 1.0f) <= 0.1f;
        }
        if (!bObservedUnitNormal) return Fail("normal-semantic");
        ++Result.PassedProbeCount;
    }
    if (!Request.Depth)
        return Fail("depth-attachment");
    if (Request.Depth)
    {
        if (!Request.Depth->IsValid() ||
            Request.Depth->Width != Request.Color->Width ||
            Request.Depth->Height != Request.Color->Height)
            return Fail("depth-attachment");
        const auto [MinimumDepth, MaximumDepth] = std::minmax_element(
            Request.Depth->LinearRgb.begin(), Request.Depth->LinearRgb.end());
        if (MaximumDepth == Request.Depth->LinearRgb.end() ||
            *MaximumDepth - *MinimumDepth <= 1.0f / 65535.0f)
            return Fail("depth-semantic");
        ++Result.PassedProbeCount;
    }
    Result.bPassed = true;
    return Result;
}

FProductionFlipResult CompareProductionImagesWithFlip(
    const FProductionCanonicalImage& Reference,
    const FProductionCanonicalImage& Candidate,
    const FProductionFlipPolicy& Policy)
{
    FProductionFlipResult Result;
    if (!Reference.IsValid() || !Candidate.IsValid() ||
        Reference.Width != Candidate.Width ||
        Reference.Height != Candidate.Height)
    {
        Result.FailureReason = "image-contract";
        return Result;
    }
    if (!IsUnit(Policy.MeanMax) || !IsUnit(Policy.P95Max) ||
        !IsUnit(Policy.MaximumMax) || !IsUnit(Policy.BadPixelThreshold) ||
        Policy.BadPixelThreshold <= 0.0f ||
        !IsUnit(Policy.BadPixelFractionMax))
    {
        Result.FailureReason = "flip-policy";
        return Result;
    }

    FLIP::Parameters Parameters;
    float Mean = 0.0f;
    float* RawErrors = nullptr;
    FLIP::evaluate(
        Reference.LinearRgb.data(), Candidate.LinearRgb.data(),
        static_cast<int>(Reference.Width), static_cast<int>(Reference.Height),
        false, Parameters, false, true, Mean, &RawErrors);
    std::unique_ptr<float[]> Errors(RawErrors);
    if (!Errors)
    {
        Result.FailureReason = "flip-evaluation";
        return Result;
    }
    const Stoner::Core::usize Count =
        static_cast<Stoner::Core::usize>(Reference.Width) * Reference.Height;
    Stoner::Core::TArray<float> Ordered(Errors.get(), Errors.get() + Count);
    if (!std::all_of(Ordered.begin(), Ordered.end(), IsUnit))
    {
        Result.FailureReason = "flip-non-finite";
        return Result;
    }
    std::sort(Ordered.begin(), Ordered.end());
    const Stoner::Core::usize P95Index = Count == 0 ? 0 :
        static_cast<Stoner::Core::usize>(std::ceil(Count * 0.95)) - 1u;
    const auto Bad = std::count_if(Ordered.begin(), Ordered.end(),
        [&Policy](float Value) { return Value > Policy.BadPixelThreshold; });
    Result.bMeasured = true;
    Result.Mean = Mean;
    Result.P95 = Ordered[P95Index];
    Result.Maximum = Ordered.back();
    Result.BadPixelFraction = static_cast<float>(Bad) /
        static_cast<float>(Count);
    Result.bPassed = Result.Mean <= Policy.MeanMax &&
        Result.P95 <= Policy.P95Max &&
        Result.Maximum <= Policy.MaximumMax &&
        Result.BadPixelFraction <= Policy.BadPixelFractionMax;
    if (!Result.bPassed) Result.FailureReason = "flip-threshold";
    return Result;
}

bool ValidateProductionNativeImageEvidence(
    const FProductionNativeImageEvidence& Evidence,
    Stoner::Core::FString& OutFailure)
{
    OutFailure = {};
    const auto Fail = [&OutFailure](const char* Reason) {
        OutFailure = Reason;
        return false;
    };
    if (Evidence.RequestedBackend.IsEmpty() ||
        Evidence.ExecutedBackend != Evidence.RequestedBackend)
        return Fail("backend-substitution");
    if (Evidence.RuntimeMode != Stoner::Core::FString("native") ||
        !Evidence.bNativeExecution)
        return Fail("native-execution");
    if (!Evidence.bSubmissionCompleted || !Evidence.bGpuReadback)
        return Fail("gpu-completion");
    if (Evidence.WorkloadRevision.IsEmpty() ||
        Evidence.WorkloadRevision != Evidence.BaselineWorkloadRevision)
        return Fail("workload-revision");
    if (Evidence.bPresented && !Evidence.bWindowOnlyCapture)
        return Fail("window-only-capture");
    return true;
}
