#include "FGLTFContainerPreflight.h"

#include <array>
#include <cctype>
#include <limits>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

constexpr std::array<Core::uint8, 4> GLBMagic = {'g', 'l', 'T', 'F'};
constexpr Core::uint32 GLBVersion = 2;
constexpr Core::uint32 JSONChunkType = 0x4E4F534A;
constexpr Core::uint32 BinaryChunkType = 0x004E4942;
constexpr Core::uint64 GLBHeaderLength = 12;
constexpr Core::uint64 GLBChunkHeaderLength = 8;

Core::uint32 ReadUint32(
    std::span<const Core::uint8> Bytes,
    Core::uint64 Offset) noexcept
{
    return static_cast<Core::uint32>(Bytes[Offset]) |
        (static_cast<Core::uint32>(Bytes[Offset + 1]) << 8U) |
        (static_cast<Core::uint32>(Bytes[Offset + 2]) << 16U) |
        (static_cast<Core::uint32>(Bytes[Offset + 3]) << 24U);
}

bool CanRead(
    Core::uint64 Offset,
    Core::uint64 Length,
    Core::uint64 Total) noexcept
{
    return Offset <= Total && Length <= Total - Offset;
}

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetResult Result,
    const char* Code,
    const char* Field)
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
    Diagnostic.Participant = Core::FString("container.gltf");
    Diagnostic.Field = Core::FString(Field);
    Diagnostics->push_back(std::move(Diagnostic));
}

bool IsValidUtf8(std::span<const Core::uint8> Bytes) noexcept
{
    for (Core::usize Index = 0; Index < Bytes.size();)
    {
        const Core::uint8 First = Bytes[Index];
        if (First <= 0x7F)
        {
            if (First == 0)
            {
                return false;
            }
            ++Index;
            continue;
        }
        Core::uint32 ContinuationCount = 0;
        Core::uint32 MinimumCodePoint = 0;
        Core::uint32 CodePoint = 0;
        if (First >= 0xC2 && First <= 0xDF)
        {
            ContinuationCount = 1;
            MinimumCodePoint = 0x80;
            CodePoint = First & 0x1FU;
        }
        else if (First >= 0xE0 && First <= 0xEF)
        {
            ContinuationCount = 2;
            MinimumCodePoint = 0x800;
            CodePoint = First & 0x0FU;
        }
        else if (First >= 0xF0 && First <= 0xF4)
        {
            ContinuationCount = 3;
            MinimumCodePoint = 0x10000;
            CodePoint = First & 0x07U;
        }
        else
        {
            return false;
        }
        if (ContinuationCount > Bytes.size() - Index - 1)
        {
            return false;
        }
        for (Core::uint32 Continuation = 0;
             Continuation < ContinuationCount;
             ++Continuation)
        {
            const Core::uint8 Byte = Bytes[Index + 1 + Continuation];
            if ((Byte & 0xC0U) != 0x80U)
            {
                return false;
            }
            CodePoint = (CodePoint << 6U) | (Byte & 0x3FU);
        }
        if (CodePoint < MinimumCodePoint ||
            (CodePoint >= 0xD800 && CodePoint <= 0xDFFF) ||
            CodePoint > 0x10FFFF)
        {
            return false;
        }
        Index += ContinuationCount + 1;
    }
    return true;
}

bool HasJsonObjectPrefix(std::span<const Core::uint8> Bytes) noexcept
{
    for (const Core::uint8 Byte : Bytes)
    {
        if (!std::isspace(static_cast<unsigned char>(Byte)))
        {
            return Byte == '{';
        }
    }
    return false;
}

bool HasJsonObjectEnvelope(std::span<const Core::uint8> Bytes) noexcept
{
    if (!HasJsonObjectPrefix(Bytes))
    {
        return false;
    }
    for (Core::usize Index = Bytes.size(); Index > 0; --Index)
    {
        const Core::uint8 Byte = Bytes[Index - 1];
        if (!std::isspace(static_cast<unsigned char>(Byte)))
        {
            return Byte == '}';
        }
    }
    return false;
}

} // namespace

EAssetResult PreflightGLTFContainer(
    std::span<const Core::uint8> Bytes,
    const FStaticModelImportLimits& Limits,
    FGLTFContainerPreflightResult& OutResult,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutResult = {};
    if (OutDiagnostics != nullptr)
    {
        OutDiagnostics->clear();
    }
    if (Limits.Validate() != EAssetResult::Success)
    {
        AddDiagnostic(OutDiagnostics, EAssetResult::InvalidInput,
            "asset.gltf.invalid-limits", "limits");
        return EAssetResult::InvalidInput;
    }
    if (Bytes.empty())
    {
        AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
            "asset.gltf.empty", "source");
        return EAssetResult::MalformedContainer;
    }
    if (Bytes.size() > Limits.MaxMainSourceBytes)
    {
        AddDiagnostic(OutDiagnostics, EAssetResult::CapacityExceeded,
            "asset.gltf.source-limit", "sourceBytes");
        return EAssetResult::CapacityExceeded;
    }

    const bool bGLB = Bytes.size() >= GLBMagic.size() &&
        std::equal(GLBMagic.begin(), GLBMagic.end(), Bytes.begin());
    if (!bGLB)
    {
        if (!IsValidUtf8(Bytes) || !HasJsonObjectEnvelope(Bytes))
        {
            AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
                "asset.gltf.json", "json");
            return EAssetResult::MalformedContainer;
        }
        OutResult.Type = EGLTFContainerType::JSON;
        OutResult.JsonLength = Bytes.size();
        return EAssetResult::Success;
    }

    if (Bytes.size() < GLBHeaderLength ||
        ReadUint32(Bytes, 4) != GLBVersion ||
        ReadUint32(Bytes, 8) != Bytes.size())
    {
        AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
            "asset.gltf.glb-header", "header");
        return EAssetResult::MalformedContainer;
    }

    Core::uint64 Offset = GLBHeaderLength;
    bool bSeenJson = false;
    bool bSeenBinary = false;
    FGLTFContainerPreflightResult Parsed;
    Parsed.Type = EGLTFContainerType::GLB;
    while (Offset < Bytes.size())
    {
        if (!CanRead(Offset, GLBChunkHeaderLength, Bytes.size()))
        {
            AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
                "asset.gltf.glb-chunk-header", "chunkHeader");
            return EAssetResult::MalformedContainer;
        }
        const Core::uint32 Length = ReadUint32(Bytes, Offset);
        const Core::uint32 Type = ReadUint32(Bytes, Offset + 4);
        const Core::uint64 DataOffset = Offset + GLBChunkHeaderLength;
        if (Length == 0 || Length % 4 != 0 ||
            !CanRead(DataOffset, Length, Bytes.size()))
        {
            AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
                "asset.gltf.glb-chunk-range", "chunkLength");
            return EAssetResult::MalformedContainer;
        }
        if (Type == JSONChunkType && !bSeenJson && !bSeenBinary)
        {
            const std::span<const Core::uint8> Json = Bytes.subspan(
                static_cast<Core::usize>(DataOffset), Length);
            if (!IsValidUtf8(Json) || !HasJsonObjectEnvelope(Json))
            {
                AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
                    "asset.gltf.glb-json", "jsonChunk");
                return EAssetResult::MalformedContainer;
            }
            Parsed.JsonOffset = DataOffset;
            Parsed.JsonLength = Length;
            bSeenJson = true;
        }
        else if (Type == BinaryChunkType && bSeenJson && !bSeenBinary)
        {
            Parsed.BinaryOffset = DataOffset;
            Parsed.BinaryLength = Length;
            bSeenBinary = true;
        }
        else
        {
            AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
                "asset.gltf.glb-chunk-order", "chunkType");
            return EAssetResult::MalformedContainer;
        }
        Offset = DataOffset + Length;
    }
    if (!bSeenJson || Offset != Bytes.size())
    {
        AddDiagnostic(OutDiagnostics, EAssetResult::MalformedContainer,
            "asset.gltf.glb-json-required", "jsonChunk");
        return EAssetResult::MalformedContainer;
    }
    OutResult = Parsed;
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
