#include "ProductionImageCalibrationTests.h"

#include "ProductionImageAcceptance.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

using namespace Stoner::Core;

void Record(
    FProductionImageCalibrationTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

bool LoadPpm(
    const std::filesystem::path& Path,
    FProductionCanonicalImage& OutImage)
{
    std::ifstream Input(Path, std::ios::binary);
    std::string Magic;
    uint32 Width = 0;
    uint32 Height = 0;
    uint32 Maximum = 0;
    if (!(Input >> Magic >> Width >> Height >> Maximum) || Magic != "P6" ||
        Width == 0 || Height == 0 || Maximum != 255)
        return false;
    Input.get();
    TArray<uint8> Rgb(static_cast<usize>(Width) * Height * 3u);
    Input.read(reinterpret_cast<char*>(Rgb.data()),
        static_cast<std::streamsize>(Rgb.size()));
    if (!Input || Input.peek() != std::ifstream::traits_type::eof())
        return false;
    TArray<uint8> Rgba(static_cast<usize>(Width) * Height * 4u);
    for (usize Pixel = 0; Pixel < Rgb.size() / 3u; ++Pixel)
    {
        Rgba[Pixel * 4u] = Rgb[Pixel * 3u];
        Rgba[Pixel * 4u + 1u] = Rgb[Pixel * 3u + 1u];
        Rgba[Pixel * 4u + 2u] = Rgb[Pixel * 3u + 2u];
        Rgba[Pixel * 4u + 3u] = 255;
    }
    FString Failure;
    return NormalizeProductionReadback(
        {Rgba, Width, Height, static_cast<uint32>(Width * 4u),
         EProductionReadbackPixelFormat::RGBA8UNorm,
         EProductionImageOrigin::TopLeft, EProductionColorTransfer::SRGB},
        OutImage, Failure);
}

FProductionFlipPolicy MeasurementPolicy()
{
    return {1.0f, 1.0f, 1.0f, 0.05f, 1.0f};
}

FProductionFlipPolicy CandidatePolicy(
    const FProductionFlipResult& Noise)
{
    return {
        std::max(0.0005f, Noise.Mean * 1.5f + 0.0001f),
        std::max(0.001f, Noise.P95 * 1.5f + 0.0001f),
        std::max(0.01f, Noise.Maximum * 1.5f + 0.001f),
        0.05f,
        std::max(0.001f, Noise.BadPixelFraction * 1.5f + 0.0001f)};
}

FProductionCanonicalImage VerticalFlip(
    const FProductionCanonicalImage& Source)
{
    FProductionCanonicalImage Result = Source;
    const usize Row = static_cast<usize>(Source.Width) * 3u;
    for (uint32 Y = 0; Y < Source.Height; ++Y)
        std::copy_n(
            Source.LinearRgb.begin() +
                static_cast<usize>(Source.Height - 1u - Y) * Row,
            Row, Result.LinearRgb.begin() + static_cast<usize>(Y) * Row);
    return Result;
}

FProductionCanonicalImage TranslateOnePixelRight(
    const FProductionCanonicalImage& Source)
{
    FProductionCanonicalImage Result = Source;
    std::fill(Result.LinearRgb.begin(), Result.LinearRgb.end(), 0.0f);
    for (uint32 Y = 0; Y < Source.Height; ++Y)
        for (uint32 X = 1; X < Source.Width; ++X)
        {
            const usize Destination =
                (static_cast<usize>(Y) * Source.Width + X) * 3u;
            const usize Original = Destination - 3u;
            std::copy_n(Source.LinearRgb.begin() + Original, 3u,
                Result.LinearRgb.begin() + Destination);
        }
    return Result;
}

FProductionCanonicalImage MissingGeometry(
    const FProductionCanonicalImage& Source)
{
    FProductionCanonicalImage Result = Source;
    for (uint32 Y = Source.Height / 8u; Y < Source.Height * 7u / 8u; ++Y)
        for (uint32 X = Source.Width / 8u; X < Source.Width * 7u / 8u; ++X)
        {
            const usize Pixel =
                (static_cast<usize>(Y) * Source.Width + X) * 3u;
            Result.LinearRgb[Pixel] = 0.0f;
            Result.LinearRgb[Pixel + 1u] = 0.0f;
            Result.LinearRgb[Pixel + 2u] = 0.0f;
        }
    return Result;
}

FProductionCanonicalImage MaterialSwap(
    const FProductionCanonicalImage& Source)
{
    FProductionCanonicalImage Result = Source;
    for (usize Pixel = 0; Pixel < Result.LinearRgb.size(); Pixel += 3u)
        std::swap(Result.LinearRgb[Pixel], Result.LinearRgb[Pixel + 2u]);
    return Result;
}

FProductionCanonicalImage WrongColorSpace(
    const FProductionCanonicalImage& Source)
{
    FProductionCanonicalImage Result = Source;
    for (float& Value : Result.LinearRgb)
        Value = std::sqrt(std::clamp(Value, 0.0f, 1.0f));
    return Result;
}

std::string JsonNumber(float Value)
{
    std::ostringstream Stream;
    Stream << std::fixed << std::setprecision(8) << Value;
    return Stream.str();
}

} // namespace

FProductionImageCalibrationTestResult RunProductionImageCalibrationTests()
{
    FProductionImageCalibrationTestResult Result;
    const char* RootValue = std::getenv("STONER_PRODUCTION_CALIBRATION_ROOT");
    const char* Backend = std::getenv("STONER_PRODUCTION_CALIBRATION_BACKEND");
    if (!RootValue || !Backend)
    {
        Record(Result, true,
            "production image calibration is controlled unavailable");
        return Result;
    }

    const std::filesystem::path Root =
        std::filesystem::path(RootValue) / Backend;
    TArray<FProductionCanonicalImage> Captures;
    for (uint32 Index = 0; Index < 20; ++Index)
    {
        std::ostringstream Name;
        Name << "capture-" << std::setw(2) << std::setfill('0') << Index
             << ".ppm";
        FProductionCanonicalImage Image;
        if (!LoadPpm(Root / Name.str(), Image))
        {
            Record(Result, false,
                "calibration loads exactly twenty canonical captures");
            return Result;
        }
        Captures.push_back(std::move(Image));
    }
    const bool SameDimensions = std::all_of(
        Captures.begin(), Captures.end(), [&Captures](const auto& Image)
        {
            return Image.Width == 512 && Image.Height == 512 &&
                Image.Width == Captures.front().Width &&
                Image.Height == Captures.front().Height;
        });
    Record(Result, SameDimensions,
        "calibration loads exactly twenty canonical captures");
    if (!SameDimensions) return Result;

    FProductionFlipResult Noise;
    Noise.bMeasured = true;
    for (usize Index = 1; Index < Captures.size(); ++Index)
    {
        const auto Measured = CompareProductionImagesWithFlip(
            Captures.front(), Captures[Index], MeasurementPolicy());
        if (!Measured.bMeasured)
        {
            Record(Result, false,
                "calibration measures same-revision FLIP noise");
            return Result;
        }
        Noise.Mean = std::max(Noise.Mean, Measured.Mean);
        Noise.P95 = std::max(Noise.P95, Measured.P95);
        Noise.Maximum = std::max(Noise.Maximum, Measured.Maximum);
        Noise.BadPixelFraction = std::max(
            Noise.BadPixelFraction, Measured.BadPixelFraction);
    }
    Record(Result, true, "calibration measures same-revision FLIP noise");
    const FProductionFlipPolicy Policy = CandidatePolicy(Noise);

    FProductionCanonicalImage Blank = Captures.front();
    std::fill(Blank.LinearRgb.begin(), Blank.LinearRgb.end(), 0.0f);
    const std::pair<const char*, FProductionCanonicalImage> Mutations[] = {
        {"blank", std::move(Blank)},
        {"origin", VerticalFlip(Captures.front())},
        {"translation-one-pixel", TranslateOnePixelRight(Captures.front())},
        {"missing-geometry", MissingGeometry(Captures.front())},
        {"material-swap", MaterialSwap(Captures.front())},
        {"color-space", WrongColorSpace(Captures.front())},
    };
    bool bMutationsRejected = true;
    for (const auto& [Name, Mutation] : Mutations)
    {
        const auto Compared = CompareProductionImagesWithFlip(
            Captures.front(), Mutation, Policy);
        bMutationsRejected = Compared.bMeasured && !Compared.bPassed &&
            bMutationsRejected;
        std::cout << "[MUTATION] name=" << Name
                  << " mean=" << JsonNumber(Compared.Mean)
                  << " p95=" << JsonNumber(Compared.P95)
                  << " max=" << JsonNumber(Compared.Maximum)
                  << " bad-fraction="
                  << JsonNumber(Compared.BadPixelFraction) << '\n';
    }
    Record(Result, bMutationsRejected,
        "candidate FLIP policy rejects the required image mutations");

    FProductionSemanticProbeRequest Stale;
    Stale.Color = &Captures.front();
    Stale.ExpectedFrameToken = 20;
    Stale.ObservedFrameToken = 19;
    const auto StaleResult = RunProductionSemanticProbes(Stale);
    Record(Result, !StaleResult.bPassed &&
        StaleResult.FirstFailure == FString("current-frame"),
        "stale frame is rejected before FLIP calibration");

    std::cout << "[CALIBRATION] {\"backend\":\"" << Backend
              << "\",\"captures\":20,\"width\":"
              << Captures.front().Width << ",\"height\":"
              << Captures.front().Height
              << ",\"noise\":{\"mean\":" << JsonNumber(Noise.Mean)
              << ",\"p95\":" << JsonNumber(Noise.P95)
              << ",\"maximum\":" << JsonNumber(Noise.Maximum)
              << ",\"badPixelFraction\":"
              << JsonNumber(Noise.BadPixelFraction)
              << "},\"policy\":{\"meanMax\":"
              << JsonNumber(Policy.MeanMax) << ",\"p95Max\":"
              << JsonNumber(Policy.P95Max) << ",\"maximumMax\":"
              << JsonNumber(Policy.MaximumMax)
              << ",\"badPixelThreshold\":"
              << JsonNumber(Policy.BadPixelThreshold)
              << ",\"badPixelFractionMax\":"
              << JsonNumber(Policy.BadPixelFractionMax) << "}}\n";
    return Result;
}
