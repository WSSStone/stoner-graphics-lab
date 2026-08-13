#include "AssetGLTFContainerTests.h"

#include "FCgltfDocument.h"
#include "FGLTFAccessorDecoder.h"
#include "FGLTFContainerPreflight.h"

#include <cmath>
#include <iostream>
#include <cstring>
#include <string>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace Stoner::Asset::Private;

void Record(FAssetGLTFContainerTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void AppendUint32(TArray<uint8>& Bytes, uint32 Value)
{
    for (uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes.push_back(static_cast<uint8>(Value >> Shift));
    }
}

void AppendFloat(TArray<uint8>& Bytes, float Value)
{
    const usize Start = Bytes.size();
    Bytes.resize(Start + sizeof(Value));
    std::memcpy(Bytes.data() + Start, &Value, sizeof(Value));
}

TArray<uint8> MakeGLB(bool IncludeBinary = true)
{
    std::string Json = "{\"asset\":{\"version\":\"2.0\"}}";
    while (Json.size() % 4 != 0)
    {
        Json.push_back(' ');
    }
    TArray<uint8> Bytes;
    Bytes.insert(Bytes.end(), {'g', 'l', 'T', 'F'});
    AppendUint32(Bytes, 2);
    AppendUint32(Bytes, 0);
    AppendUint32(Bytes, static_cast<uint32>(Json.size()));
    AppendUint32(Bytes, 0x4E4F534A);
    Bytes.insert(Bytes.end(), Json.begin(), Json.end());
    if (IncludeBinary)
    {
        AppendUint32(Bytes, 4);
        AppendUint32(Bytes, 0x004E4942);
        Bytes.insert(Bytes.end(), {0, 0, 0, 0});
    }
    const uint32 Length = static_cast<uint32>(Bytes.size());
    for (uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes[8 + Shift / 8] = static_cast<uint8>(Length >> Shift);
    }
    return Bytes;
}

EAssetResult Preflight(
    const TArray<uint8>& Bytes,
    FGLTFContainerPreflightResult& OutResult)
{
    return PreflightGLTFContainer(Bytes, {}, OutResult);
}

} // namespace

FAssetGLTFContainerTestResult RunAssetGLTFContainerTests()
{
    FAssetGLTFContainerTestResult Result;
    FGLTFContainerPreflightResult Parsed;
    const TArray<uint8> Json = {
        '{', '"', 'a', 's', 's', 'e', 't', '"', ':',
        '{', '"', 'v', 'e', 'r', 's', 'i', 'o', 'n', '"', ':',
        '"', '2', '.', '0', '"', '}', '}'};
    Record(Result,
        Preflight(Json, Parsed) == EAssetResult::Success &&
            Parsed.Type == EGLTFContainerType::JSON && Parsed.JsonLength == Json.size(),
        "JSON glTF preflight records bounded JSON range");

    const TArray<uint8> GLB = MakeGLB();
    Record(Result,
        Preflight(GLB, Parsed) == EAssetResult::Success &&
            Parsed.Type == EGLTFContainerType::GLB && Parsed.JsonLength != 0 &&
            Parsed.BinaryLength == 4,
        "GLB preflight accepts ordered JSON and BIN chunks");

    FStaticModelImportProfile Profile;
    FCgltfDocument Document;
    Record(Result,
        FCgltfDocument::Parse(Json, Profile, Document) == EAssetResult::Success &&
            Document.GetNativeDocument() != nullptr &&
            Document.GetParserAllocatedBytes() != 0,
        "cgltf document parses bounded memory without filesystem callbacks");
    FCgltfDocument Moved = std::move(Document);
    Record(Result,
        Document.GetNativeDocument() == nullptr &&
            Moved.GetNativeDocument() != nullptr,
        "cgltf document ownership remains valid after move");

    Record(Result,
        FCgltfDocument::Parse(GLB, Profile, Document) == EAssetResult::Success &&
            Document.GetNativeDocument() != nullptr,
        "cgltf document parses bounded GLB source in memory");

    TArray<uint8> BadMagic = GLB;
    BadMagic[0] = 'x';
    Record(Result,
        Preflight(BadMagic, Parsed) == EAssetResult::MalformedContainer,
        "wrong GLB magic cannot fall through as JSON");

    TArray<uint8> BadLength = GLB;
    ++BadLength[8];
    Record(Result,
        Preflight(BadLength, Parsed) == EAssetResult::MalformedContainer,
        "GLB declared length rejects trailing or missing bytes");

    TArray<uint8> BadAlignment = GLB;
    BadAlignment[12] = 3;
    Record(Result,
        Preflight(BadAlignment, Parsed) == EAssetResult::MalformedContainer,
        "GLB chunk lengths require four-byte alignment");

    TArray<uint8> Reordered = GLB;
    Reordered[16] = 0x42;
    Reordered[17] = 0x49;
    Reordered[18] = 0x4E;
    Reordered[19] = 0x00;
    Record(Result,
        Preflight(Reordered, Parsed) == EAssetResult::MalformedContainer,
        "GLB requires the JSON chunk before BIN");

    TArray<uint8> BadPadding = MakeGLB(false);
    BadPadding[BadPadding.size() - 1] = 'x';
    Record(Result,
        Preflight(BadPadding, Parsed) == EAssetResult::MalformedContainer,
        "GLB JSON padding only permits trailing whitespace");

    TArray<uint8> EmbeddedNul = Json;
    EmbeddedNul.insert(EmbeddedNul.begin() + 1, 0);
    Record(Result,
        Preflight(EmbeddedNul, Parsed) == EAssetResult::MalformedContainer,
        "JSON preflight rejects embedded NUL bytes");

    FStaticModelImportProfile TinyAllocationProfile;
    TinyAllocationProfile.Limits.MaxParserAllocationBytes = 1;
    Record(Result,
        FCgltfDocument::Parse(Json, TinyAllocationProfile, Document) ==
            EAssetResult::CapacityExceeded &&
            Document.GetNativeDocument() == nullptr,
        "cgltf parser allocation cap leaves no partial document");

    TArray<uint8> DenseBytes;
    AppendFloat(DenseBytes, 1.0f);
    AppendFloat(DenseBytes, 2.0f);
    AppendFloat(DenseBytes, 3.0f);
    FGLTFAccessorSource Dense;
    Dense.Type = EGLTFAccessorType::Vec3;
    Dense.Count = 1;
    Dense.BaseBytes = DenseBytes;
    FGLTFDecodedAccessor Decoded;
    Record(Result,
        DecodeGLTFAccessorToFloat(Dense, 16, Decoded) == EAssetResult::Success &&
            Decoded.Values == TArray<float>{1.0f, 2.0f, 3.0f},
        "dense float accessor decodes canonical components");

    TArray<uint8> Interleaved;
    AppendFloat(Interleaved, 1.0f);
    AppendFloat(Interleaved, 2.0f);
    AppendFloat(Interleaved, 3.0f);
    AppendFloat(Interleaved, -1.0f);
    AppendFloat(Interleaved, 4.0f);
    AppendFloat(Interleaved, 5.0f);
    AppendFloat(Interleaved, 6.0f);
    AppendFloat(Interleaved, -1.0f);
    Dense.Count = 2;
    Dense.BaseBytes = Interleaved;
    Dense.ByteStride = 16;
    Record(Result,
        DecodeGLTFAccessorToFloat(Dense, 16, Decoded) == EAssetResult::Success &&
            Decoded.Values == TArray<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f},
        "interleaved accessor honors byte stride");

    FGLTFAccessorSource Normalized;
    Normalized.ComponentType = EGLTFComponentType::Int8;
    Normalized.Type = EGLTFAccessorType::Vec3;
    Normalized.Count = 1;
    Normalized.bNormalized = true;
    const TArray<uint8> NormalizedBytes = {0x80, 0x00, 0x7F};
    Normalized.BaseBytes = NormalizedBytes;
    Record(Result,
        DecodeGLTFAccessorToFloat(Normalized, 16, Decoded) == EAssetResult::Success &&
            Decoded.Values == TArray<float>{-1.0f, 0.0f, 1.0f},
        "normalized signed integer accessor follows glTF endpoints");

    const TArray<uint8> IntermediateNormalizedBytes = {0xC0, 0x40};
    Normalized.Type = EGLTFAccessorType::Vec2;
    Normalized.BaseBytes = IntermediateNormalizedBytes;
    Record(Result,
        DecodeGLTFAccessorToFloat(Normalized, 16, Decoded) == EAssetResult::Success &&
            Decoded.Values.size() == 2 &&
            std::fabs(Decoded.Values[0] - (-64.0f / 127.0f)) < 0.000001f &&
            std::fabs(Decoded.Values[1] - (64.0f / 127.0f)) < 0.000001f,
        "normalized signed integer accessor preserves intermediate values");

    const TArray<uint8> IntermediateNormalizedInt16Bytes = {
        0x00, 0xC0, 0x00, 0x40};
    Normalized.ComponentType = EGLTFComponentType::Int16;
    Normalized.BaseBytes = IntermediateNormalizedInt16Bytes;
    Record(Result,
        DecodeGLTFAccessorToFloat(Normalized, 16, Decoded) == EAssetResult::Success &&
            Decoded.Values.size() == 2 &&
            std::fabs(Decoded.Values[0] - (-16384.0f / 32767.0f)) < 0.000001f &&
            std::fabs(Decoded.Values[1] - (16384.0f / 32767.0f)) < 0.000001f,
        "normalized signed 16-bit accessor preserves intermediate values");

    FGLTFAccessorSource Sparse;
    Sparse.Type = EGLTFAccessorType::Vec2;
    Sparse.Count = 3;
    Sparse.bHasBaseBufferView = false;
    const TArray<uint8> SparseIndices = {1};
    TArray<uint8> SparseValues;
    AppendFloat(SparseValues, 7.0f);
    AppendFloat(SparseValues, 9.0f);
    Sparse.Sparse = FGLTFSparseAccessorSource{
        1,
        EGLTFComponentType::UInt8,
        SparseIndices,
        0,
        SparseValues,
        0};
    Record(Result,
        DecodeGLTFAccessorToFloat(Sparse, 16, Decoded) == EAssetResult::Success &&
            Decoded.Values == TArray<float>{0.0f, 0.0f, 7.0f, 9.0f, 0.0f, 0.0f},
        "sparse accessor overlays a zero base deterministically");
    Sparse.Sparse->Count = 2;
    const TArray<uint8> DuplicateSparseIndices = {1, 1};
    const TArray<uint8> DuplicateSparseValues = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0};
    Sparse.Sparse->Indices = DuplicateSparseIndices;
    Sparse.Sparse->Values = DuplicateSparseValues;
    Record(Result,
        DecodeGLTFAccessorToFloat(Sparse, 16, Decoded) == EAssetResult::MalformedSource &&
            Decoded.Values.empty(),
        "sparse indices must be strictly increasing and unique");

    FGLTFAccessorSource Indices;
    Indices.ComponentType = EGLTFComponentType::UInt16;
    Indices.Count = 3;
    const TArray<uint8> IndexBytes = {0, 0, 1, 0, 0xFF, 0xFF};
    Indices.BaseBytes = IndexBytes;
    TArray<uint32> DecodedIndices;
    Record(Result,
        DecodeGLTFAccessorToIndices(Indices, 16, DecodedIndices) == EAssetResult::Success &&
            DecodedIndices == TArray<uint32>{0, 1, 65535},
        "unsigned index accessor preserves 8 16 and 32-bit domains");
    const TArray<uint8> SparseIndexPositions = {1};
    const TArray<uint8> SparseIndexValues = {2, 0};
    const TArray<uint8> SparseIndexBase = {0, 0, 0, 0, 0, 0};
    Indices.BaseBytes = SparseIndexBase;
    Indices.Sparse = FGLTFSparseAccessorSource{
        1,
        EGLTFComponentType::UInt8,
        SparseIndexPositions,
        0,
        SparseIndexValues,
        0};
    Record(Result,
        DecodeGLTFAccessorToIndices(Indices, 16, DecodedIndices) == EAssetResult::Success &&
            DecodedIndices == TArray<uint32>{0, 2, 0},
        "sparse index accessor applies ordered unsigned overrides");
    Indices.bHasBaseBufferView = false;
    Indices.BaseBytes = {};
    Indices.ByteStride = 0;
    Record(Result,
        DecodeGLTFAccessorToIndices(Indices, 16, DecodedIndices) == EAssetResult::Success &&
            DecodedIndices == TArray<uint32>{0, 2, 0},
        "sparse-only index accessor overlays an implicit zero base");
    Indices.bHasBaseBufferView = true;
    Indices.BaseBytes = SparseIndexBase;
    Indices.Sparse.reset();
    Indices.ByteStride = 3;
    Record(Result,
        DecodeGLTFAccessorToIndices(Indices, 16, DecodedIndices) ==
            EAssetResult::MalformedSource && DecodedIndices.empty(),
        "accessor stride alignment and ranges fail before output publication");
    return Result;
}
