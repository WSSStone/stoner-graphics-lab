#include "AssetImageTextureTests.h"

#include "Asset/AssetMinimal.h"
#include "FImageDecode.h"
#include "FImageMipGenerator.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <thread>
#include <vector>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

class FMemoryImageSource final : public IAssetSource
{
public:
    explicit FMemoryImageSource(
        TArray<uint8> Bytes,
        EAssetResult ReadResult = EAssetResult::Success)
        : Bytes_(std::move(Bytes))
        , ReadResult_(ReadResult)
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (ReadResult_ != EAssetResult::Success)
        {
            return ReadResult_;
        }
        if (Offset > Bytes_.size())
        {
            return EAssetResult::MalformedSource;
        }
        const usize Count = std::min(
            MaximumBytes,
            Bytes_.size() - static_cast<usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return EAssetResult::Success;
    }

private:
    TArray<uint8> Bytes_;
    EAssetResult ReadResult_;
};

void Record(
    FAssetImageTextureTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

TArray<uint8> ReadFixture(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>()};
}

FAssetId MakeId(
    const char* Type,
    const std::string& Path,
    const char* Subresource)
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString(Type),
        FString(Path),
        std::optional<FString>(FString(Subresource)),
        Id);
    return Id;
}

FAssetSourceLocator MakeSource(const std::string& Path)
{
    FAssetSourceLocator Locator;
    (void)FAssetSourceLocator::Create(
        FString("fixture"),
        FString(Path),
        Locator);
    return Locator;
}

FAssetImportRequest MakeRequest(
    const std::string& Name,
    TArray<uint8> Bytes,
    FImageImportSettings Settings = {},
    std::optional<FString> Hint = std::nullopt,
    EAssetResult ReadResult = EAssetResult::Success,
    bool PreserveUnspecifiedSemantic = false)
{
    FAssetImportRequest Request;
    Request.Descriptor.Location = MakeSource(Name);
    Request.Descriptor.Size = Bytes.size();
    Request.Descriptor.FormatHint = std::move(Hint);
    Request.Source = FAssetSourceLease(
        MakeShared<FMemoryImageSource>(std::move(Bytes), ReadResult));
    auto Parameters = MakeShared<FImageImportParameters>();
    Parameters->ImageId = MakeId("Image", "Tests/" + Name, "image");
    Parameters->TextureId = MakeId("Texture", "Tests/" + Name, "texture");
    if (!PreserveUnspecifiedSemantic &&
        Settings.Semantic == ETextureSemantic::Unspecified)
    {
        Settings.Semantic = ETextureSemantic::Color;
    }
    Parameters->Settings = std::move(Settings);
    Request.Parameters = std::move(Parameters);
    return Request;
}

bool GenerateSyntheticMips(
    FImageExtent2D Extent,
    EImageTexelFormat Format,
    TArray<uint8> Bytes,
    FImageImportSettings Settings,
    TArray<FImageMip>& OutMips)
{
    FImageMip Base;
    return FImageMip::Create(
               Extent,
               Format,
               std::move(Bytes),
               Base) == EAssetResult::Success &&
        Private::GenerateImageMips(
            Base,
            Settings,
            OutMips) == EAssetResult::Success;
}

FImageImportResult Import(
    const FAssetImportRequest& Request,
    FAssetRegistry& Registry)
{
    FAssetExtensionRegistry Extensions;
    FAssetRegistrationToken Token;
    if (RegisterImageAssetImporter(Extensions, Token) !=
        EAssetResult::Success)
    {
        return {};
    }
    return FAssetImportService::ImportAndRegister(
        Extensions,
        Registry,
        Request);
}

std::vector<std::filesystem::path> ValidFixtures()
{
    std::vector<std::filesystem::path> Paths;
    const std::filesystem::path Root = "Tests/Fixtures/Images/Valid";
    for (const auto& Entry : std::filesystem::directory_iterator(Root))
    {
        if (Entry.is_regular_file())
        {
            Paths.push_back(Entry.path());
        }
    }
    std::sort(Paths.begin(), Paths.end());
    return Paths;
}

void TestValidCorpus(FAssetImageTextureTestResult& Result)
{
    const auto Paths = ValidFixtures();
    bool AllValid = Paths.size() >= 12;
    bool Deterministic = true;
    for (const auto& Path : Paths)
    {
        const TArray<uint8> Bytes = ReadFixture(Path);
        FAssetRegistry Registry;
        const FAssetImportRequest Request =
            MakeRequest(Path.filename().string(), Bytes);
        const FImageImportResult First = Import(Request, Registry);
        if (First.Result != EAssetResult::Success)
        {
            std::cout << "  fixture failure: " << Path.string()
                      << " result=" << static_cast<int>(First.Result) << '\n';
        }
        AllValid = AllValid &&
            First.Result == EAssetResult::Success &&
            First.Image &&
            First.Texture &&
            First.Image->GetOrigin() == EImageOrigin::TopLeft &&
            !First.Image->GetBaseMip().GetBytes().empty() &&
            First.Texture->GetMips().front().GetBytes().data() ==
                First.Image->GetBaseMip().GetBytes().data() &&
            Registry.Snapshot().Records.size() == 2;
        if (!First.Image || !First.Texture)
        {
            continue;
        }
        const FString FirstDiagnostics =
            FAssetDiagnostics::FormatNormalized(First.Diagnostics);
        const FString FirstRegistry =
            FAssetInspection::Format(Registry.Snapshot());
        for (int Iteration = 0; Iteration < 20; ++Iteration)
        {
            FAssetRegistry RepeatedRegistry;
            const FImageImportResult Repeated =
                Import(Request, RepeatedRegistry);
            Deterministic = Deterministic &&
                Repeated.Result == EAssetResult::Success &&
                Repeated.Image &&
                Repeated.Texture &&
                Repeated.Image->GetContentDigest() ==
                    First.Image->GetContentDigest() &&
                Repeated.Texture->GetContentDigest() ==
                    First.Texture->GetContentDigest() &&
                FAssetDiagnostics::FormatNormalized(
                    Repeated.Diagnostics) == FirstDiagnostics &&
                FAssetInspection::Format(
                    RepeatedRegistry.Snapshot()) == FirstRegistry &&
                Repeated.Texture->GetMips().size() ==
                    First.Texture->GetMips().size() &&
                Repeated.Image->GetBaseMip().GetBytes().size() ==
                    First.Image->GetBaseMip().GetBytes().size() &&
                std::equal(
                    Repeated.Image->GetBaseMip().GetBytes().begin(),
                    Repeated.Image->GetBaseMip().GetBytes().end(),
                    First.Image->GetBaseMip().GetBytes().begin());
            if (Repeated.Texture->GetMips().size() ==
                First.Texture->GetMips().size())
            {
                for (usize MipIndex = 0;
                     MipIndex < First.Texture->GetMips().size();
                     ++MipIndex)
                {
                    Deterministic = Deterministic &&
                        Repeated.Texture->GetMips()[MipIndex].GetExtent() ==
                            First.Texture->GetMips()[MipIndex].GetExtent() &&
                        std::equal(
                            Repeated.Texture->GetMips()[MipIndex]
                                .GetBytes().begin(),
                            Repeated.Texture->GetMips()[MipIndex]
                                .GetBytes().end(),
                            First.Texture->GetMips()[MipIndex]
                                .GetBytes().begin());
                }
            }
        }
    }
    Record(Result, AllValid, "all 12+ PNG JPEG HDR fixtures import atomically");
    Record(Result, Deterministic, "valid corpus is exact over 20 repeated imports");
}

void TestProbeAndOrientation(FAssetImageTextureTestResult& Result)
{
    const auto PngPath =
        std::filesystem::path("Tests/Fixtures/Images/Valid/png-rgb-3x5.png");
    const TArray<uint8> Png = ReadFixture(PngPath);
    bool HintsPass = true;
    for (const auto& Hint : {
             std::optional<FString>{},
             std::optional<FString>(FString("PNG")),
             std::optional<FString>(FString("misleading.txt"))})
    {
        FAssetRegistry Registry;
        HintsPass = HintsPass &&
            Import(MakeRequest("probe.png", Png, {}, Hint), Registry).Result ==
                EAssetResult::Success;
    }
    Record(Result, HintsPass, "content probing tolerates absent uppercase and misleading hints");

    const TArray<uint8> Oriented = ReadFixture(
        "Tests/Fixtures/Images/Valid/png-exif-o6-2x3.png");
    FAssetRegistry Registry;
    const FImageImportResult Imported =
        Import(MakeRequest("oriented.png", Oriented), Registry);
    const std::array<uint8, 3> Expected = {55, 195, 127};
    Record(
        Result,
        Imported.Image &&
            Imported.Image->GetBaseMip().GetExtent() ==
                FImageExtent2D{3, 2} &&
            Imported.Image->GetBaseMip().GetBytes().size() >= 3 &&
            std::equal(
                Expected.begin(),
                Expected.end(),
                Imported.Image->GetBaseMip().GetBytes().begin()),
        "PNG eXIf orientation normalizes to exact top-left first pixel");

    const TArray<uint8> Jpeg = ReadFixture(
        "Tests/Fixtures/Images/Valid/jpeg-exif-o6-2x3.jpg");
    FAssetRegistry JpegRegistry;
    const FImageImportResult JpegImported =
        Import(MakeRequest("oriented.jpg", Jpeg), JpegRegistry);
    if (!JpegImported.Image)
    {
        std::cout << "  JPEG orientation failure result="
                  << static_cast<int>(JpegImported.Result) << '\n';
    }
    else if (JpegImported.Image->GetBaseMip().GetExtent() !=
             FImageExtent2D{3, 2})
    {
        std::cout << "  JPEG orientation extent="
                  << JpegImported.Image->GetBaseMip().GetExtent().Width
                  << "x"
                  << JpegImported.Image->GetBaseMip().GetExtent().Height
                  << '\n';
    }
    Record(
        Result,
        JpegImported.Image &&
            JpegImported.Image->GetBaseMip().GetExtent() ==
                FImageExtent2D{3, 2},
        "JPEG APP1 orientation normalizes extent to top-left");
}

void TestHdrLayouts(FAssetImageTextureTestResult& Result)
{
    const TArray<uint8> Hdr =
        ReadFixture("Tests/Fixtures/Images/Valid/hdr-rgb-3x2.hdr");
    std::array<EHDRLayout, 3> Layouts = {
        EHDRLayout::DefaultRGBA16F,
        EHDRLayout::RGBA32F,
        EHDRLayout::RGB32F};
    std::array<EImageTexelFormat, 3> Formats = {
        EImageTexelFormat::R16G16B16A16_Float,
        EImageTexelFormat::R32G32B32A32_Float,
        EImageTexelFormat::R32G32B32_Float};
    std::array<FAssetDigest, 3> Digests;
    bool Passed = true;
    for (usize Index = 0; Index < Layouts.size(); ++Index)
    {
        FImageImportSettings Settings;
        Settings.HDRLayout = Layouts[Index];
        FAssetRegistry Registry;
        const FImageImportResult Imported =
            Import(MakeRequest("layout.hdr", Hdr, Settings), Registry);
        Passed = Passed &&
            Imported.Image &&
            Imported.Image->GetBaseMip().GetFormat() == Formats[Index];
        if (Imported.Texture)
        {
            Digests[Index] = Imported.Texture->GetContentDigest();
        }
    }
    Passed = Passed &&
        Digests[0] != Digests[1] &&
        Digests[0] != Digests[2] &&
        Digests[1] != Digests[2];
    Record(Result, Passed, "HDR layouts select exact formats and distinct versions");
}

void TestMipSemantics(FAssetImageTextureTestResult& Result)
{
    const TArray<uint8> Png =
        ReadFixture("Tests/Fixtures/Images/Valid/png-rgb-3x5.png");
    FAssetRegistry Registry;
    const FImageImportResult Color =
        Import(MakeRequest("mips.png", Png), Registry);
    bool Recurrence = Color.Texture != nullptr;
    if (Color.Texture)
    {
        FImageExtent2D Expected = Color.Image->GetBaseMip().GetExtent();
        for (const FImageMip& Mip : Color.Texture->GetMips())
        {
            Recurrence = Recurrence && Mip.GetExtent() == Expected;
            Expected = {
                std::max(1U, Expected.Width / 2U),
                std::max(1U, Expected.Height / 2U)};
        }
        Recurrence = Recurrence &&
            Color.Texture->GetMips().back().GetExtent() ==
                FImageExtent2D{1, 1};
    }
    Record(Result, Recurrence, "full mip chain follows fixed odd extent recurrence");

    FImageImportSettings BaseOnly;
    BaseOnly.MipPolicy = EImageMipPolicy::BaseOnly;
    FAssetRegistry BaseRegistry;
    const FImageImportResult Base =
        Import(MakeRequest("base.png", Png, BaseOnly), BaseRegistry);
    Record(
        Result,
        Base.Texture && Base.Texture->GetMips().size() == 1,
        "explicit base-only policy retains one level");

    FImageImportSettings InvalidNormal;
    InvalidNormal.Semantic = ETextureSemantic::Normal;
    InvalidNormal.ColorSpace = EImageColorSpace::SRGB;
    FAssetRegistry InvalidRegistry;
    Record(
        Result,
        Import(
            MakeRequest("invalid-normal.png", Png, InvalidNormal),
            InvalidRegistry).Result == EAssetResult::InvalidInput &&
            InvalidRegistry.Snapshot().Records.empty(),
        "sRGB normal input fails before publication");

    FImageImportSettings Srgb;
    Srgb.Semantic = ETextureSemantic::Color;
    Srgb.ColorSpace = EImageColorSpace::SRGB;
    TArray<FImageMip> SrgbMips;
    const TArray<uint8> ColorPixels = {
        0, 0, 0, 0,
        255, 255, 255, 64,
        0, 0, 0, 128,
        255, 255, 255, 255};
    const std::array<uint8, 4> ExpectedSrgb = {188, 188, 188, 112};
    Record(
        Result,
        GenerateSyntheticMips(
            {2, 2},
            EImageTexelFormat::R8G8B8A8_UNorm,
            ColorPixels,
            Srgb,
            SrgbMips) &&
            SrgbMips.size() == 2 &&
            std::equal(
                ExpectedSrgb.begin(),
                ExpectedSrgb.end(),
                SrgbMips[1].GetBytes().begin()),
        "sRGB RGB filters in linear light while straight alpha averages independently");

    FImageImportSettings LinearColor;
    LinearColor.Semantic = ETextureSemantic::Color;
    LinearColor.ColorSpace = EImageColorSpace::Linear;
    TArray<FImageMip> LinearMips;
    const std::array<uint8, 4> ExpectedLinear = {128, 128, 128, 112};
    Record(
        Result,
        GenerateSyntheticMips(
            {2, 2},
            EImageTexelFormat::R8G8B8A8_UNorm,
            ColorPixels,
            LinearColor,
            LinearMips) &&
            std::equal(
                ExpectedLinear.begin(),
                ExpectedLinear.end(),
                LinearMips[1].GetBytes().begin()),
        "linear color and alpha match exact scalar reference bytes");

    FImageImportSettings Data;
    Data.Semantic = ETextureSemantic::Data;
    Data.ColorSpace = EImageColorSpace::Linear;
    TArray<FImageMip> DataMips;
    const std::array<uint8, 2> ExpectedData = {115, 50};
    Record(
        Result,
        GenerateSyntheticMips(
            {3, 1},
            EImageTexelFormat::R8G8_UNorm,
            {0, 10, 90, 40, 255, 100},
            Data,
            DataMips) &&
            DataMips.size() == 2 &&
            std::equal(
                ExpectedData.begin(),
                ExpectedData.end(),
                DataMips[1].GetBytes().begin()),
        "generic data channels use exact independent odd-width filtering");

    FImageImportSettings Normal;
    Normal.Semantic = ETextureSemantic::Normal;
    Normal.ColorSpace = EImageColorSpace::Linear;
    TArray<FImageMip> NormalMips;
    const std::array<uint8, 3> ExpectedFallback = {128, 128, 255};
    Record(
        Result,
        GenerateSyntheticMips(
            {2, 2},
            EImageTexelFormat::R8G8B8_UNorm,
            {
                255, 128, 128, 0, 127, 127,
                255, 128, 128, 0, 127, 127},
            Normal,
            NormalMips) &&
            std::equal(
                ExpectedFallback.begin(),
                ExpectedFallback.end(),
                NormalMips[1].GetBytes().begin()),
        "indeterminate normal average uses deterministic encoded +Z fallback");

    TArray<FImageMip> SinglePixelMips;
    Record(
        Result,
        GenerateSyntheticMips(
            {1, 1},
            EImageTexelFormat::R8_UNorm,
            {37},
            Data,
            SinglePixelMips) &&
            SinglePixelMips.size() == 1 &&
            SinglePixelMips[0].GetBytes()[0] == 37,
        "one-pixel full chain terminates without a duplicate level");
}

void TestValidationAndDiagnostics(FAssetImageTextureTestResult& Result)
{
    const TArray<uint8> Valid =
        ReadFixture("Tests/Fixtures/Images/Valid/png-rgba-5x3.png");

    FAssetRegistry MissingSemanticRegistry;
    const FImageImportResult MissingSemantic = Import(
        MakeRequest(
            "missing-semantic.png",
            Valid,
            {},
            std::nullopt,
            EAssetResult::Success,
            true),
        MissingSemanticRegistry);
    Record(
        Result,
        MissingSemantic.Result == EAssetResult::InvalidInput &&
            MissingSemanticRegistry.Snapshot().Records.empty() &&
            !MissingSemantic.Diagnostics.empty() &&
            MissingSemantic.Diagnostics.front().Stage ==
                EAssetStage::Validate,
        "missing explicit semantic fails with normalized validation evidence");

    FImageImportSettings InvalidEnum;
    InvalidEnum.Semantic = ETextureSemantic::Color;
    InvalidEnum.MipPolicy = static_cast<EImageMipPolicy>(255);
    FAssetRegistry InvalidEnumRegistry;
    Record(
        Result,
        Import(
            MakeRequest("invalid-enum.png", Valid, InvalidEnum),
            InvalidEnumRegistry).Result == EAssetResult::InvalidInput &&
            InvalidEnumRegistry.Snapshot().Records.empty(),
        "unknown settings enum values fail closed");

    FImageImportSettings SourceLimit;
    SourceLimit.Semantic = ETextureSemantic::Color;
    SourceLimit.Limits.MaxSourceBytes = Valid.size() - 1;
    FAssetRegistry LimitRegistry;
    const FImageImportResult Limited = Import(
        MakeRequest("limited.png", Valid, SourceLimit),
        LimitRegistry);
    const auto LimitDiagnostic = std::find_if(
        Limited.Diagnostics.begin(),
        Limited.Diagnostics.end(),
        [](const FAssetDiagnostic& Diagnostic)
        {
            return Diagnostic.Result == EAssetResult::ImageLimitExceeded;
        });
    Record(
        Result,
        Limited.Result == EAssetResult::ImageLimitExceeded &&
            LimitRegistry.Snapshot().Records.empty() &&
            LimitDiagnostic != Limited.Diagnostics.end() &&
            !LimitDiagnostic->Subject.IsEmpty() &&
            !LimitDiagnostic->Participant.IsEmpty() &&
            !LimitDiagnostic->Field.IsEmpty() &&
            !LimitDiagnostic->Limit.IsEmpty(),
        "source limit failure carries source participant field and limit");

    const TArray<uint8> BadCrc = ReadFixture(
        "Tests/Fixtures/Images/Invalid/png-bad-crc.png");
    FAssetRegistry BadCrcRegistry;
    const FImageImportResult BadCrcResult =
        Import(MakeRequest("bad-crc.png", BadCrc), BadCrcRegistry);
    const auto InspectDiagnostic = std::find_if(
        BadCrcResult.Diagnostics.begin(),
        BadCrcResult.Diagnostics.end(),
        [](const FAssetDiagnostic& Diagnostic)
        {
            return Diagnostic.Stage == EAssetStage::Inspect &&
                Diagnostic.Code == FString("image.png.bad-crc");
        });
    Record(
        Result,
        BadCrcResult.Result == EAssetResult::MalformedSource &&
            InspectDiagnostic != BadCrcResult.Diagnostics.end() &&
            !InspectDiagnostic->Subject.IsEmpty() &&
            !InspectDiagnostic->Participant.IsEmpty() &&
            InspectDiagnostic->Field == FString("crc"),
        "container failure preserves stable inspect-stage diagnostic details");

    FImageImportSettings ChainLimit;
    ChainLimit.Semantic = ETextureSemantic::Data;
    ChainLimit.ColorSpace = EImageColorSpace::Linear;
    ChainLimit.Limits.MaxDecodedChainBytes = 4;
    FImageMip Base;
    TArray<FImageMip> Mips;
    FAssetDiagnostic Diagnostic;
    const bool BaseCreated = FImageMip::Create(
        {2, 2},
        EImageTexelFormat::R8_UNorm,
        {1, 2, 3, 4},
        Base) == EAssetResult::Success;
    const EAssetResult ChainResult = BaseCreated
        ? Private::GenerateImageMips(
              Base,
              ChainLimit,
              Mips,
              &Diagnostic)
        : EAssetResult::ProcessingFailure;
    Record(
        Result,
        ChainResult == EAssetResult::ImageLimitExceeded &&
            Mips.empty() &&
            Diagnostic.Stage == EAssetStage::Mip &&
            Diagnostic.Limit == FString("4"),
        "decoded-chain first value above configured limit fails atomically");

    uint16 Half = 0;
    Record(
        Result,
        Private::EncodeHalf(
            std::numeric_limits<float>::infinity(),
            Half) == EAssetResult::NonFiniteImageData &&
            Private::EncodeHalf(
                std::numeric_limits<float>::quiet_NaN(),
                Half) == EAssetResult::NonFiniteImageData &&
            Private::EncodeHalf(
                65505.0f,
                Half) == EAssetResult::HDRPrecisionRangeExceeded,
        "HDR half conversion distinguishes non-finite and precision-range failures");

    FAssetRegistry ConflictRegistry;
    const FImageImportResult Initial = Import(
        MakeRequest("conflict.png", Valid),
        ConflictRegistry);
    const TArray<uint8> Different =
        ReadFixture("Tests/Fixtures/Images/Valid/png-rgb-3x5.png");
    const FImageImportResult Conflict = Import(
        MakeRequest("conflict.png", Different),
        ConflictRegistry);
    Record(
        Result,
        Initial.Result == EAssetResult::Success &&
            Conflict.Result != EAssetResult::Success &&
            !Conflict.Image &&
            !Conflict.Texture &&
            ConflictRegistry.Snapshot().Records.size() == 2 &&
            std::any_of(
                Conflict.Diagnostics.begin(),
                Conflict.Diagnostics.end(),
                [](const FAssetDiagnostic& Diagnostic)
                {
                    return Diagnostic.Stage == EAssetStage::Registry &&
                        Diagnostic.Code ==
                            FString("image.registry.publication");
                }),
        "registry conflict publishes neither replacement and reports registry stage");
}

void TestFailureMatrix(FAssetImageTextureTestResult& Result)
{
    const TArray<uint8> Valid =
        ReadFixture("Tests/Fixtures/Images/Valid/png-rgba-5x3.png");
    int Rejected = 0;
    for (usize Size = 0; Size < 28; ++Size)
    {
        TArray<uint8> Truncated(
            Valid.begin(),
            Valid.begin() + static_cast<std::ptrdiff_t>(
                std::min(Size, Valid.size())));
        FAssetRegistry Registry;
        const FImageImportResult Imported =
            Import(MakeRequest("truncated.png", Truncated), Registry);
        if (Imported.Result != EAssetResult::Success &&
            Registry.Snapshot().Records.empty())
        {
            ++Rejected;
        }
    }
    for (const char* Name : {
             "unsupported.bin",
             "truncated-png.bin",
             "png-bad-crc.png",
             "hdr-bad-header.hdr"})
    {
        FAssetRegistry Registry;
        const TArray<uint8> Bytes = ReadFixture(
            std::filesystem::path("Tests/Fixtures/Images/Invalid") / Name);
        const FImageImportResult Imported =
            Import(MakeRequest(Name, Bytes), Registry);
        if (Imported.Result != EAssetResult::Success &&
            Registry.Snapshot().Records.empty())
        {
            ++Rejected;
        }
    }
    Record(
        Result,
        Rejected >= 30,
        "30+ malformed mutation cases reject without partial publication");

    FAssetRegistry MissingRegistry;
    FAssetImportRequest Missing =
        MakeRequest("missing.png", {}, {}, FString("png"));
    Missing.Descriptor.Size = 1;
    Missing.Source = FAssetSourceLease(
        MakeShared<FMemoryImageSource>(
            TArray<uint8>{},
            EAssetResult::NotFound));
    FAssetRegistry DeniedRegistry;
    FAssetImportRequest Denied =
        MakeRequest("denied.png", {}, {}, FString("png"));
    Denied.Descriptor.Size = 1;
    Denied.Source = FAssetSourceLease(
        MakeShared<FMemoryImageSource>(
            TArray<uint8>{},
            EAssetResult::AccessDenied));
    Record(
        Result,
        Import(Missing, MissingRegistry).Result == EAssetResult::NotFound &&
            Import(Denied, DeniedRegistry).Result ==
                EAssetResult::AccessDenied,
        "missing and access-denied sources remain distinct");
}

void TestConcurrency(FAssetImageTextureTestResult& Result)
{
    const TArray<uint8> Png =
        ReadFixture("Tests/Fixtures/Images/Valid/png-rgb-3x5.png");
    const FAssetImportRequest Request =
        MakeRequest("concurrent.png", Png);
    std::array<FAssetDigest, 8> Digests;
    std::array<EAssetResult, 8> Results;
    std::vector<std::thread> Threads;
    for (usize Index = 0; Index < Results.size(); ++Index)
    {
        Threads.emplace_back([&, Index]
        {
            FAssetRegistry Registry;
            const FImageImportResult Imported = Import(Request, Registry);
            Results[Index] = Imported.Result;
            if (Imported.Texture)
            {
                Digests[Index] = Imported.Texture->GetContentDigest();
            }
        });
    }
    for (auto& Thread : Threads)
    {
        Thread.join();
    }
    bool Passed = true;
    for (usize Index = 0; Index < Results.size(); ++Index)
    {
        Passed = Passed &&
            Results[Index] == EAssetResult::Success &&
            Digests[Index] == Digests[0];
    }
    Record(Result, Passed, "eight concurrent immutable imports are identical");
}

} // namespace

FAssetImageTextureTestResult RunAssetImageTextureTests()
{
    FAssetImageTextureTestResult Result;
    TestValidCorpus(Result);
    TestProbeAndOrientation(Result);
    TestHdrLayouts(Result);
    TestMipSemantics(Result);
    TestValidationAndDiagnostics(Result);
    TestFailureMatrix(Result);
    TestConcurrency(Result);
    return Result;
}
