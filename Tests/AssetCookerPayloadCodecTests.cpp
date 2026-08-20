#include "AssetCookerPayloadCodecTests.h"

#include "Asset/FAssetCookContractCodec.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

void Record(
    FAssetCookerPayloadCodecTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetId MakeId()
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString("Image"), FString("Cooker/Test"), {}, Id);
    return Id;
}

FAssetCookedPayloadHeader MakeHeader()
{
    FAssetCookedPayloadHeader Header;
    Header.AssetId = MakeId();
    Header.AssetType = FString("Image");
    Header.CodecId = FString("stoner.image");
    Header.CodecVersion = 1;
    Header.PayloadSchemaVersion = 1;
    Header.BodyBytes = 1;
    const TArray<uint8> Placeholder = {0};
    Header.BodyDigest = FAssetDigest::FromBytes(Placeholder);
    return Header;
}

TArray<uint8> MakeEnvelope(FAssetCookedPayloadEnvelope* Out = nullptr)
{
    const TArray<uint8> Body = {0, 1, 2, 3, 0xfe, 0xff};
    TArray<uint8> Bytes;
    (void)FAssetCookContractCodec::WriteCookedPayload(
        MakeHeader(), {}, Body, {}, Bytes, Out);
    return Bytes;
}

usize FindText(const TArray<uint8>& Bytes, std::string_view Text)
{
    const auto Iterator = std::search(
        Bytes.begin(), Bytes.end(), Text.begin(), Text.end());
    return Iterator == Bytes.end()
        ? Bytes.size()
        : static_cast<usize>(Iterator - Bytes.begin());
}

TArray<uint8> ReadFixture(const char* Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
}

void TestRoundTrip(FAssetCookerPayloadCodecTestResult& Result)
{
    FAssetCookedPayloadEnvelope Written;
    const TArray<uint8> Bytes = MakeEnvelope(&Written);
    FAssetCookedPayloadEnvelope Parsed;
    const EAssetResult ParseResult = FAssetCookContractCodec::ParseCookedPayload(
        Bytes, {}, Parsed);
    Record(
        Result,
        ParseResult == EAssetResult::Success && Parsed == Written &&
            Bytes.size() > 16 && Bytes[8] == 1 && Bytes[9] == 0 &&
            Parsed.Header.BodyBytes == Parsed.Body.size() &&
            Parsed.EnvelopeDigest == FAssetDigest::FromBytes(Bytes),
        "SGCOOK01 round-trip preserves exact little-endian envelope evidence");

    TArray<uint8> Rewritten;
    FAssetCookedPayloadEnvelope Second;
    (void)FAssetCookContractCodec::WriteCookedPayload(
        Parsed.Header,
        Parsed.ReservedHeaderExtensions,
        Parsed.Body,
        {},
        Rewritten,
        &Second);
    Record(
        Result,
        Rewritten == Bytes && Second == Parsed,
        "repeated envelope writing is byte-identical");
}

void TestCorruption(FAssetCookerPayloadCodecTestResult& Result)
{
    const TArray<uint8> Baseline = MakeEnvelope();
    bool AllTruncationsRejected = true;
    for (usize Size = 0; Size < Baseline.size(); ++Size)
    {
        FAssetCookedPayloadEnvelope Parsed;
        if (FAssetCookContractCodec::ParseCookedPayload(
                std::span<const uint8>(Baseline.data(), Size), {}, Parsed) ==
            EAssetResult::Success)
        {
            AllTruncationsRejected = false;
            break;
        }
    }
    Record(Result, AllTruncationsRejected, "every truncated envelope is rejected");

    TArray<uint8> BodyMutation = Baseline;
    BodyMutation.back() ^= 0x01U;
    FAssetCookedPayloadEnvelope Parsed;
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(
            BodyMutation, {}, Parsed) == EAssetResult::CorruptPayload,
        "body substitution is detected by SHA-256");

    TArray<uint8> Extra = Baseline;
    Extra.push_back(0);
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(Extra, {}, Parsed) !=
            EAssetResult::Success,
        "trailing bytes are rejected");
}

void TestUnknownContracts(FAssetCookerPayloadCodecTestResult& Result)
{
    const TArray<uint8> Baseline = MakeEnvelope();
    FAssetCookedPayloadEnvelope Parsed;

    TArray<uint8> Container = Baseline;
    Container[8] = 2;
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(Container, {}, Parsed) ==
            EAssetResult::UnsupportedSchema,
        "unknown container version fails closed");

    TArray<uint8> Codec = Baseline;
    const usize CodecOffset = FindText(Codec, "stoner.image");
    Codec[CodecOffset + 7] = 'x';
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(Codec, {}, Parsed) ==
            EAssetResult::Unsupported,
        "unknown payload codec fails closed");

    TArray<uint8> CodecVersion = Baseline;
    const usize CodecVersionOffset = CodecOffset + 12;
    CodecVersion[CodecVersionOffset] = 2;
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(
            CodecVersion, {}, Parsed) == EAssetResult::UnsupportedSchema,
        "unknown payload codec version fails closed");

    TArray<uint8> Schema = Baseline;
    const usize SchemaOffset = CodecOffset + 12 + 4;
    Schema[SchemaOffset] = 2;
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(Schema, {}, Parsed) ==
            EAssetResult::UnsupportedSchema,
        "unknown payload schema fails closed");

    TArray<uint8> Type = Baseline;
    const usize TypeOffset = FindText(Type, "Image");
    Type[TypeOffset] = 'M';
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(Type, {}, Parsed) !=
            EAssetResult::Success,
        "asset identity and declared type mismatch is rejected");
}

void TestBounds(FAssetCookerPayloadCodecTestResult& Result)
{
    const TArray<uint8> Baseline = MakeEnvelope();
    FAssetCookedPayloadLimits Limits;
    Limits.MaxEnvelopeBytes = Baseline.size() - 1;
    Limits.MaxBodyBytes = 1024;
    FAssetCookedPayloadEnvelope Parsed;
    Record(
        Result,
        FAssetCookContractCodec::ParseCookedPayload(Baseline, Limits, Parsed) !=
            EAssetResult::Success && Parsed.Body.empty(),
        "envelope limit rejects before publishing partial output");

    TArray<uint8> Empty;
    const TArray<uint8> NoBody;
    Record(
        Result,
        FAssetCookContractCodec::WriteCookedPayload(
            MakeHeader(), {}, NoBody, {}, Empty) != EAssetResult::Success &&
            Empty.empty(),
        "writer rejects an empty body without partial bytes");
}

void TestMetalShaderEnvelopeV2(FAssetCookerPayloadCodecTestResult& Result)
{
    FAssetCookedPayloadHeader Header;
    (void)FAssetId::Create(
        FString("ShaderPayload"), FString("Cooker/Metal/Test"), {},
        Header.AssetId);
    Header.AssetType = FString("ShaderPayload");
    Header.CodecId = FString("stoner.shader-payload");
    Header.CodecVersion = 2;
    Header.PayloadSchemaVersion = 2;
    const TArray<uint8> Body = {0x4d, 0x54, 0x4c, 0x42};
    TArray<uint8> Envelope;
    FAssetCookedPayloadEnvelope Written;
    const EAssetResult Write = FAssetCookContractCodec::WriteCookedPayload(
        Header, {}, Body, {}, Envelope, &Written);
    FAssetCookedPayloadEnvelope Parsed;
    const EAssetResult Parse = FAssetCookContractCodec::ParseCookedPayload(
        Envelope, {}, Parsed);
    Record(
        Result,
        Write == EAssetResult::Success && Parse == EAssetResult::Success &&
            Parsed == Written && Parsed.Header.CodecVersion == 2 &&
            Parsed.Header.PayloadSchemaVersion == 2,
        "shader payload v2 envelope round-trips without weakening v1 codecs");

    Header.PayloadSchemaVersion = 1;
    Envelope.clear();
    Record(
        Result,
        FAssetCookContractCodec::WriteCookedPayload(
            Header, {}, Body, {}, Envelope) != EAssetResult::Success &&
            Envelope.empty(),
        "mixed shader payload codec and schema revisions fail closed");
}

void TestGoldenFixtures(FAssetCookerPayloadCodecTestResult& Result)
{
    constexpr const char* Root =
        "Tests/Fixtures/AssetCooker/Contracts/Payloads/";
    const TArray<uint8> Valid = ReadFixture(
        "Tests/Fixtures/AssetCooker/Contracts/Payloads/valid-image.sgasset");
    FAssetCookedPayloadEnvelope Parsed;
    const bool ValidPassed = !Valid.empty() &&
        FAssetCookContractCodec::ParseCookedPayload(Valid, {}, Parsed) ==
            EAssetResult::Success &&
        Parsed.Header.CodecId == FString("stoner.image");
    bool InvalidRejected = true;
    for (const char* Name : {
             "invalid-truncated.sgasset", "invalid-body-digest.sgasset"})
    {
        const TArray<uint8> Invalid = ReadFixture(
            (std::string(Root) + Name).c_str());
        InvalidRejected = InvalidRejected && !Invalid.empty() &&
            FAssetCookContractCodec::ParseCookedPayload(
                Invalid, {}, Parsed) != EAssetResult::Success;
    }
    Record(Result, ValidPassed, "checked-in envelope golden is accepted");
    Record(Result, InvalidRejected,
        "checked-in malformed envelope goldens are rejected");
}

} // namespace

FAssetCookerPayloadCodecTestResult RunAssetCookerPayloadCodecTests()
{
    FAssetCookerPayloadCodecTestResult Result;
    TestRoundTrip(Result);
    TestCorruption(Result);
    TestUnknownContracts(Result);
    TestBounds(Result);
    TestMetalShaderEnvelopeV2(Result);
    TestGoldenFixtures(Result);
    return Result;
}
