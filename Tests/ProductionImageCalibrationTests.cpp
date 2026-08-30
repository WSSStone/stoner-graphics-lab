#include "ProductionImageCalibrationTests.h"

#include "ProductionImageAcceptance.h"
#include "ProductionImageReference.h"
#include "ProductionNativeImageAcceptance.h"
#include "../ThirdParty/yyjson/yyjson.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
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
    const FProductionCanonicalImage& Source,
    const FProductionPixelRegion& Region)
{
    FProductionCanonicalImage Result = Source;
    for (uint32 Y = Region.MinimumY; Y < Region.MaximumYExclusive; ++Y)
        for (uint32 X = Region.MinimumX; X < Region.MaximumXExclusive; ++X)
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

bool ParseSubmissionFrameToken(yyjson_val* Value, uint64& OutToken)
{
    if (!yyjson_is_str(Value)) return false;
    const std::string_view Text(yyjson_get_str(Value), yyjson_get_len(Value));
    constexpr std::string_view Prefix = "submission-";
    if (!Text.starts_with(Prefix)) return false;
    const std::string_view Digits = Text.substr(Prefix.size());
    if (Digits.empty() || Digits.front() == '0') return false;
    const auto Parsed = std::from_chars(
        Digits.data(), Digits.data() + Digits.size(), OutToken);
    return Parsed.ec == std::errc{} &&
        Parsed.ptr == Digits.data() + Digits.size() && OutToken != 0;
}

bool ReadCapturedFrameTokens(
    const std::filesystem::path& Path,
    uint64& OutObserved,
    uint64& OutExpected)
{
    std::ifstream Input(Path, std::ios::binary);
    if (!Input) return false;
    std::string Bytes{std::istreambuf_iterator<char>(Input), {}};
    yyjson_doc* Document = yyjson_read(
        const_cast<char*>(Bytes.data()), Bytes.size(), YYJSON_READ_NOFLAG);
    if (!Document) return false;
    yyjson_val* Root = yyjson_doc_get_root(Document);
    const bool bParsed = yyjson_is_obj(Root) &&
        ParseSubmissionFrameToken(
            yyjson_obj_get(Root, "frameToken"), OutObserved) &&
        ParseSubmissionFrameToken(
            yyjson_obj_get(Root, "expectedFrameToken"), OutExpected);
    yyjson_doc_free(Document);
    return bParsed;
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
        FString Failure;
        if (!LoadProductionReferenceImage(
                Root / Name.str(), EProductionColorTransfer::SRGB,
                Image, Failure))
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
    const bool bCompareOnly = []
    {
        const char* Value = std::getenv(
            "STONER_PRODUCTION_CALIBRATION_COMPARE_ONLY");
        return Value && std::string_view(Value) == "1";
    }();
    if (!bCompareOnly)
    {
        const char* WorkloadRevision = std::getenv(
            "STONER_PRODUCTION_WORKLOAD_REVISION");
        TArray<FProductionRegionProbe> Regions;
        if (!WorkloadRevision || !BuildProductionWorkloadRegions(
                WorkloadRevision, Captures.front().Width,
                Captures.front().Height, Regions))
        {
            Record(Result, false,
                "calibration resolves the exact workload region contract");
            return Result;
        }
        const auto MissingRegion = std::find_if(
            Regions.begin(), Regions.end(), [](const auto& Probe)
            {
                return Probe.Name == FString("primitive-material");
            });
        if (MissingRegion == Regions.end())
        {
            Record(Result, false,
                "calibration resolves the exact workload region contract");
            return Result;
        }
        Record(Result, true,
            "calibration resolves the exact workload region contract");

        uint64 StaleObserved = 0;
        uint64 StaleExpected = 0;
        uint64 CurrentObserved = 0;
        uint64 CurrentExpected = 0;
        const bool bStaleMetadataValid = ReadCapturedFrameTokens(
                Root / "capture-00.json", StaleObserved, StaleExpected) &&
            ReadCapturedFrameTokens(
                Root / "capture-19.json", CurrentObserved, CurrentExpected) &&
            StaleObserved == StaleExpected &&
            CurrentObserved == CurrentExpected &&
            StaleObserved < CurrentObserved;
        FProductionSemanticProbeRequest StaleRequest;
        StaleRequest.Color = &Captures.front();
        StaleRequest.ExpectedFrameToken = CurrentObserved;
        StaleRequest.ObservedFrameToken = StaleObserved;
        const FProductionSemanticProbeResult StaleResult =
            RunProductionSemanticProbes(StaleRequest);
        const bool bStaleRejected = bStaleMetadataValid &&
            !StaleResult.bPassed &&
            StaleResult.FirstFailure == FString("current-frame");
        std::cout << "[MUTATION] name=stale result="
                  << (bStaleRejected ? "rejected" : "accepted") << '\n';
        Record(Result, bStaleRejected,
            "calibration rejects a real prior submitted frame token");

        TArray<FProductionCanonicalImage> References = {Captures.front()};
        if (const char* ReferenceRoots = std::getenv(
                "STONER_PRODUCTION_CALIBRATION_REFERENCE_ROOTS"))
        {
            std::stringstream Stream(ReferenceRoots);
            std::string ReferenceRoot;
            const auto CurrentRoot = Root.lexically_normal();
            while (std::getline(Stream, ReferenceRoot, ';'))
            {
                const auto CandidateRoot =
                    (std::filesystem::path(ReferenceRoot) / Backend).
                        lexically_normal();
                if (ReferenceRoot.empty() || CandidateRoot == CurrentRoot)
                    continue;
                FProductionCanonicalImage Reference;
                FString ReferenceFailure;
                if (!LoadProductionReferenceImage(
                        CandidateRoot / "capture-00.ppm",
                        EProductionColorTransfer::SRGB,
                        Reference, ReferenceFailure) ||
                    Reference.Width != Captures.front().Width ||
                    Reference.Height != Captures.front().Height)
                {
                    Record(Result, false,
                        "calibration loads every candidate reference mode");
                    return Result;
                }
                References.push_back(std::move(Reference));
            }
        }
        Record(Result, References.size() <= 3u,
            "calibration loads every candidate reference mode");
        if (References.empty() || References.size() > 3u) return Result;

        FProductionCanonicalImage Blank = Captures.front();
        std::fill(Blank.LinearRgb.begin(), Blank.LinearRgb.end(), 0.0f);
        const std::pair<const char*, FProductionCanonicalImage> Mutations[] = {
            {"blank", std::move(Blank)},
            {"origin", VerticalFlip(Captures.front())},
            {"translation-one-pixel", TranslateOnePixelRight(Captures.front())},
            {"missing-geometry", MissingGeometry(
                Captures.front(), MissingRegion->Region)},
            {"material-swap", MaterialSwap(Captures.front())},
            {"color-space", WrongColorSpace(Captures.front())},
        };
        bool bMutationsRejected = bStaleRejected;
        for (const auto& [Name, Mutation] : Mutations)
        {
            FProductionFlipResult Compared;
            bool bRejectedByAll = true;
            for (const auto& Reference : References)
            {
                Compared = CompareProductionImagesWithFlip(
                    Reference, Mutation, Policy);
                bRejectedByAll = Compared.bMeasured && !Compared.bPassed &&
                    bRejectedByAll;
            }
            bMutationsRejected = bRejectedByAll && bMutationsRejected;
            std::cout << "[MUTATION] name=" << Name
                      << " result="
                      << (bRejectedByAll ? "rejected" : "accepted")
                      << " mean=" << JsonNumber(Compared.Mean)
                      << " p95=" << JsonNumber(Compared.P95)
                      << " max=" << JsonNumber(Compared.Maximum)
                      << " bad-fraction="
                      << JsonNumber(Compared.BadPixelFraction) << '\n';
        }
        Record(Result, bMutationsRejected,
            "candidate FLIP policy rejects the required image mutations");

        const bool bOppositeNormalsRejected =
            !IsProductionWorkloadNormalProbeValid(
                "production-content-lantern-v2", FVector3::UnitX()) &&
            !IsProductionWorkloadNormalProbeValid(
                "production-content-sponza-v2", -FVector3::UnitY());
        std::cout << "[MUTATION] name=opposite-normal result="
                  << (bOppositeNormalsRejected ? "rejected" : "accepted")
                  << '\n';
        Record(Result, bOppositeNormalsRejected,
            "attachment normal-direction probes reject opposite-normal mutations");
    }

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
