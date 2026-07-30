#include "FKTX2Preflight.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

constexpr std::array<Core::uint8, 12> Identifier = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
    0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr Core::uint64 HeaderBytes = 80;
constexpr Core::uint64 LevelIndexBytes = 24;

Core::uint32 ReadU32(std::span<const Core::uint8> Bytes, Core::usize Offset)
{
    return static_cast<Core::uint32>(Bytes[Offset]) |
        (static_cast<Core::uint32>(Bytes[Offset + 1]) << 8U) |
        (static_cast<Core::uint32>(Bytes[Offset + 2]) << 16U) |
        (static_cast<Core::uint32>(Bytes[Offset + 3]) << 24U);
}

Core::uint64 ReadU64(std::span<const Core::uint8> Bytes, Core::usize Offset)
{
    return static_cast<Core::uint64>(ReadU32(Bytes, Offset)) |
        (static_cast<Core::uint64>(ReadU32(Bytes, Offset + 4)) << 32U);
}

bool Add(Core::uint64 Left, Core::uint64 Right, Core::uint64& Out)
{
    if (Right > std::numeric_limits<Core::uint64>::max() - Left)
    {
        return false;
    }
    Out = Left + Right;
    return true;
}

bool Multiply(Core::uint64 Left, Core::uint64 Right, Core::uint64& Out)
{
    if (Left != 0 &&
        Right > std::numeric_limits<Core::uint64>::max() / Left)
    {
        return false;
    }
    Out = Left * Right;
    return true;
}

struct FRange
{
    Core::uint64 Begin = 0;
    Core::uint64 End = 0;
    const char* Name = "";
};

bool AddRange(
    Core::TArray<FRange>& Ranges,
    Core::uint64 Offset,
    Core::uint64 Length,
    Core::uint64 Total,
    Core::uint64 Alignment,
    const char* Name)
{
    Core::uint64 End = 0;
    if (Length == 0)
    {
        return Offset == 0;
    }
    if ((Alignment > 1 && Offset % Alignment != 0) ||
        !Add(Offset, Length, End) || End > Total)
    {
        return false;
    }
    Ranges.push_back({Offset, End, Name});
    return true;
}

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetResult Result,
    const char* Code,
    const char* Field,
    Core::uint64 Actual,
    Core::uint64 Limit)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Container;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = Core::FString("container.ktx2");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Actual = Core::FString(std::to_string(Actual));
    if (Limit != 0)
    {
        Diagnostic.Limit = Core::FString(std::to_string(Limit));
    }
    Diagnostics->push_back(std::move(Diagnostic));
}

} // namespace

EAssetResult PreflightKTX2(
    std::span<const Core::uint8> Bytes,
    const FTextureCookLimits& Limits,
    FKTX2PreflightResult& OutResult,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutResult = {};
    if (OutDiagnostics != nullptr)
    {
        OutDiagnostics->clear();
    }
    if (Limits.Validate() != EAssetResult::Success)
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::InvalidInput,
            "asset.ktx2.invalid-limits",
            "limits",
            0,
            0);
        return EAssetResult::InvalidInput;
    }
    if (Bytes.size() > Limits.MaxArtifactBytes)
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::KTX2LimitExceeded,
            "asset.ktx2.artifact-limit",
            "artifactBytes",
            Bytes.size(),
            Limits.MaxArtifactBytes);
        return EAssetResult::KTX2LimitExceeded;
    }
    if (Bytes.size() < HeaderBytes ||
        !std::equal(Identifier.begin(), Identifier.end(), Bytes.begin()))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.header",
            "identifier",
            Bytes.size(),
            HeaderBytes);
        return EAssetResult::MalformedContainer;
    }

    FKTX2PreflightResult Parsed;
    Parsed.VkFormat = ReadU32(Bytes, 12);
    Parsed.TypeSize = ReadU32(Bytes, 16);
    Parsed.Width = ReadU32(Bytes, 20);
    Parsed.Height = ReadU32(Bytes, 24);
    const Core::uint32 Depth = ReadU32(Bytes, 28);
    const Core::uint32 Layers = ReadU32(Bytes, 32);
    const Core::uint32 Faces = ReadU32(Bytes, 36);
    Parsed.LevelCount = ReadU32(Bytes, 40);
    Parsed.Supercompression = ReadU32(Bytes, 44);
    Parsed.DfdOffset = ReadU32(Bytes, 48);
    Parsed.DfdLength = ReadU32(Bytes, 52);
    Parsed.KvdOffset = ReadU32(Bytes, 56);
    Parsed.KvdLength = ReadU32(Bytes, 60);
    Parsed.SgdOffset = ReadU64(Bytes, 64);
    Parsed.SgdLength = ReadU64(Bytes, 72);

    if (Parsed.Width == 0 || Parsed.Height == 0 ||
        Parsed.Width > Limits.MaxDimension ||
        Parsed.Height > Limits.MaxDimension ||
        Depth != 0 || Layers != 0 || Faces != 1 ||
        Parsed.LevelCount == 0 ||
        Parsed.LevelCount > Limits.MaxMipLevels)
    {
        const bool LimitExceeded =
            Parsed.Width > Limits.MaxDimension ||
            Parsed.Height > Limits.MaxDimension ||
            Parsed.LevelCount > Limits.MaxMipLevels;
        AddDiagnostic(
            OutDiagnostics,
            LimitExceeded
                ? EAssetResult::KTX2LimitExceeded
                : EAssetResult::MalformedContainer,
            "asset.ktx2.scope",
            "dimensionsOrLevels",
            std::max({
                static_cast<Core::uint64>(Parsed.Width),
                static_cast<Core::uint64>(Parsed.Height),
                static_cast<Core::uint64>(Parsed.LevelCount)}),
            std::max(
                static_cast<Core::uint64>(Limits.MaxDimension),
                static_cast<Core::uint64>(Limits.MaxMipLevels)));
        return LimitExceeded
            ? EAssetResult::KTX2LimitExceeded
            : EAssetResult::MalformedContainer;
    }
    if (Parsed.TypeSize == 0 ||
        Parsed.DfdLength == 0 ||
        (Parsed.Supercompression == 1 &&
            Parsed.SgdLength == 0) ||
        (Parsed.Supercompression == 0 &&
            Parsed.SgdLength != 0))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.header-contract",
            "typeDfdOrSgd",
            Parsed.Supercompression,
            0);
        return EAssetResult::MalformedContainer;
    }

    Core::uint64 IndexSize = 0;
    Core::uint64 ProtectedEnd = 0;
    if (!Multiply(
            Parsed.LevelCount, LevelIndexBytes, IndexSize) ||
        !Add(HeaderBytes, IndexSize, ProtectedEnd) ||
        ProtectedEnd > Bytes.size())
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.level-index",
            "levelIndex",
            Parsed.LevelCount,
            Bytes.size());
        return EAssetResult::MalformedContainer;
    }

    Core::uint64 MetadataBytes = 0;
    if (!Add(Parsed.DfdLength, Parsed.KvdLength, MetadataBytes) ||
        !Add(MetadataBytes, Parsed.SgdLength, MetadataBytes) ||
        MetadataBytes > Limits.MaxMetadataBytes)
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::KTX2LimitExceeded,
            "asset.ktx2.metadata-limit",
            "metadataBytes",
            MetadataBytes,
            Limits.MaxMetadataBytes);
        return EAssetResult::KTX2LimitExceeded;
    }

    Core::TArray<FRange> Ranges = {
        {0, ProtectedEnd, "header"}};
    if (!AddRange(
            Ranges,
            Parsed.DfdOffset,
            Parsed.DfdLength,
            Bytes.size(),
            4,
            "dfd") ||
        !AddRange(
            Ranges,
            Parsed.KvdOffset,
            Parsed.KvdLength,
            Bytes.size(),
            4,
            "kvd") ||
        !AddRange(
            Ranges,
            Parsed.SgdOffset,
            Parsed.SgdLength,
            Bytes.size(),
            8,
            "sgd"))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.metadata-range",
            "metadataRange",
            MetadataBytes,
            Bytes.size());
        return EAssetResult::MalformedContainer;
    }

    Parsed.Levels.reserve(Parsed.LevelCount);
    Core::uint32 ExpectedWidth = Parsed.Width;
    Core::uint32 ExpectedHeight = Parsed.Height;
    const Core::uint64 LevelAlignment =
        Parsed.Supercompression == 0 ? 4 : 1;
    for (Core::uint32 Level = 0;
         Level < Parsed.LevelCount;
         ++Level)
    {
        const Core::usize Offset =
            HeaderBytes + Level * LevelIndexBytes;
        FKTX2Level Description;
        Description.MipLevel = Level;
        Description.Extent = {ExpectedWidth, ExpectedHeight};
        Description.ByteOffset = ReadU64(Bytes, Offset);
        Description.ByteLength = ReadU64(Bytes, Offset + 8);
        Description.UncompressedByteLength =
            ReadU64(Bytes, Offset + 16);
        if (Description.ByteLength == 0 ||
            Description.ByteLength > Limits.MaxLevelBytes ||
            Description.UncompressedByteLength >
                Limits.MaxLevelBytes ||
            (Parsed.Supercompression == 0 &&
                Description.UncompressedByteLength !=
                    Description.ByteLength) ||
            !AddRange(
                Ranges,
                Description.ByteOffset,
                Description.ByteLength,
                Bytes.size(),
                LevelAlignment,
                "level"))
        {
            const bool LimitExceeded =
                Description.ByteLength > Limits.MaxLevelBytes ||
                Description.UncompressedByteLength >
                    Limits.MaxLevelBytes;
            AddDiagnostic(
                OutDiagnostics,
                LimitExceeded
                    ? EAssetResult::KTX2LimitExceeded
                    : EAssetResult::MalformedContainer,
                "asset.ktx2.level-range",
                "levelBytes",
                Description.ByteLength,
                Limits.MaxLevelBytes);
            if (OutDiagnostics != nullptr)
            {
                OutDiagnostics->back().Level = Level;
            }
            return LimitExceeded
                ? EAssetResult::KTX2LimitExceeded
                : EAssetResult::MalformedContainer;
        }
        Parsed.Levels.push_back(Description);
        ExpectedWidth = ExpectedWidth > 1 ? ExpectedWidth >> 1U : 1;
        ExpectedHeight =
            ExpectedHeight > 1 ? ExpectedHeight >> 1U : 1;
    }

    std::sort(
        Ranges.begin(),
        Ranges.end(),
        [](const FRange& Left, const FRange& Right)
        {
            return Left.Begin < Right.Begin;
        });
    for (Core::usize Index = 1; Index < Ranges.size(); ++Index)
    {
        if (Ranges[Index].Begin < Ranges[Index - 1].End)
        {
            AddDiagnostic(
                OutDiagnostics,
                EAssetResult::MalformedContainer,
                "asset.ktx2.overlap",
                Ranges[Index].Name,
                Ranges[Index].Begin,
                Ranges[Index - 1].End);
            return EAssetResult::MalformedContainer;
        }
    }
    OutResult = std::move(Parsed);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
