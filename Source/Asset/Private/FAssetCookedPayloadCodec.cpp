#include "FAssetCookedPayloadCodec.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

constexpr std::array<Core::uint8, 8> Magic = {
    'S', 'G', 'C', 'O', 'O', 'K', '0', '1'};

void AppendU16(Core::TArray<Core::uint8>& Out, Core::uint16 Value)
{
    Out.push_back(static_cast<Core::uint8>(Value));
    Out.push_back(static_cast<Core::uint8>(Value >> 8U));
}

void AppendU32(Core::TArray<Core::uint8>& Out, Core::uint32 Value)
{
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
        Out.push_back(static_cast<Core::uint8>(Value >> Shift));
}

void AppendU64(Core::TArray<Core::uint8>& Out, Core::uint64 Value)
{
    for (Core::uint32 Shift = 0; Shift < 64; Shift += 8)
        Out.push_back(static_cast<Core::uint8>(Value >> Shift));
}

void AppendText32(Core::TArray<Core::uint8>& Out, std::string_view Text)
{
    AppendU32(Out, static_cast<Core::uint32>(Text.size()));
    Out.insert(Out.end(), Text.begin(), Text.end());
}

void AppendText16(Core::TArray<Core::uint8>& Out, std::string_view Text)
{
    AppendU16(Out, static_cast<Core::uint16>(Text.size()));
    Out.insert(Out.end(), Text.begin(), Text.end());
}

bool CheckedAdd(Core::uint64 Left, Core::uint64 Right, Core::uint64& Out)
{
    if (Right > std::numeric_limits<Core::uint64>::max() - Left)
        return false;
    Out = Left + Right;
    return true;
}

class FReader
{
public:
    explicit FReader(std::span<const Core::uint8> Bytes)
        : Bytes_(Bytes)
    {
    }

    bool ReadU16(Core::uint16& Out)
    {
        if (!Require(2)) return false;
        Out = static_cast<Core::uint16>(Bytes_[Offset_]) |
            static_cast<Core::uint16>(Bytes_[Offset_ + 1] << 8U);
        Offset_ += 2;
        return true;
    }

    bool ReadU32(Core::uint32& Out)
    {
        if (!Require(4)) return false;
        Out = 0;
        for (Core::uint32 Index = 0; Index < 4; ++Index)
            Out |= static_cast<Core::uint32>(Bytes_[Offset_ + Index]) << (Index * 8U);
        Offset_ += 4;
        return true;
    }

    bool ReadU64(Core::uint64& Out)
    {
        if (!Require(8)) return false;
        Out = 0;
        for (Core::uint32 Index = 0; Index < 8; ++Index)
            Out |= static_cast<Core::uint64>(Bytes_[Offset_ + Index]) << (Index * 8U);
        Offset_ += 8;
        return true;
    }

    bool ReadText32(Core::FString& Out)
    {
        Core::uint32 Size = 0;
        return ReadU32(Size) && ReadText(Size, Out);
    }

    bool ReadText16(Core::FString& Out)
    {
        Core::uint16 Size = 0;
        return ReadU16(Size) && ReadText(Size, Out);
    }

    bool ReadBytes(Core::usize Count, Core::TArray<Core::uint8>& Out)
    {
        if (!Require(Count)) return false;
        Out.assign(Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset_),
                   Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset_ + Count));
        Offset_ += Count;
        return true;
    }

    bool Skip(Core::usize Count)
    {
        if (!Require(Count)) return false;
        Offset_ += Count;
        return true;
    }

    [[nodiscard]] Core::usize Offset() const noexcept { return Offset_; }

private:
    bool ReadText(Core::usize Count, Core::FString& Out)
    {
        if (Count == 0 || !Require(Count)) return false;
        const char* Begin = reinterpret_cast<const char*>(Bytes_.data() + Offset_);
        if (std::find(Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset_),
                      Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset_ + Count),
                      Core::uint8{0}) !=
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset_ + Count))
            return false;
        Out = Core::FString(std::string(Begin, Count));
        Offset_ += Count;
        return true;
    }

    bool Require(Core::usize Count) const
    {
        return Count <= Bytes_.size() - std::min(Offset_, Bytes_.size());
    }

    std::span<const Core::uint8> Bytes_;
    Core::usize Offset_ = 0;
};

EAssetResult ParseAssetId(const Core::FString& Text, FAssetId& Out)
{
    Out = {};
    const std::string_view View = Text.View();
    const std::size_t Colon = View.find(':');
    const std::size_t Hash = View.find('#', Colon == std::string_view::npos ? 0 : Colon + 1);
    if (Colon == std::string_view::npos || Colon == 0 || Colon + 1 >= View.size())
        return EAssetResult::InvalidIdentity;
    std::optional<Core::FString> Subresource;
    if (Hash != std::string_view::npos)
    {
        if (Hash + 1 >= View.size()) return EAssetResult::InvalidIdentity;
        Subresource = Core::FString(View.substr(Hash + 1));
    }
    const EAssetResult Result = FAssetId::Create(
        Core::FString(View.substr(0, Colon)),
        Core::FString(View.substr(
            Colon + 1,
            Hash == std::string_view::npos ? std::string_view::npos : Hash - Colon - 1)),
        Subresource,
        Out);
    return Result == EAssetResult::Success && Out.ToString() == Text
        ? EAssetResult::Success
        : EAssetResult::InvalidIdentity;
}

} // namespace

EAssetResult WriteAssetCookedPayload(
    const FAssetCookedPayloadHeader& Header,
    std::span<const Core::uint8> ReservedHeaderExtensions,
    std::span<const Core::uint8> Body,
    const FAssetCookedPayloadLimits& Limits,
    Core::TArray<Core::uint8>& OutBytes,
    FAssetCookedPayloadEnvelope* OutEnvelope)
{
    OutBytes.clear();
    if (OutEnvelope) *OutEnvelope = {};
    if (Limits.Validate() != EAssetResult::Success || Body.empty() ||
        Body.size() > Limits.MaxBodyBytes ||
        ReservedHeaderExtensions.size() > Limits.MaxHeaderBytes)
        return EAssetResult::InvalidInput;

    FAssetCookedPayloadHeader Normalized = Header;
    Normalized.BodyBytes = Body.size();
    Normalized.BodyDigest = FAssetDigest::FromBytes(Body);
    if (Normalized.Validate() != EAssetResult::Success)
        return EAssetResult::InvalidInput;

    Core::TArray<Core::uint8> HeaderBytes;
    HeaderBytes.insert(HeaderBytes.end(), Magic.begin(), Magic.end());
    AppendU16(HeaderBytes, Normalized.ContainerVersion);
    const Core::usize HeaderSizeOffset = HeaderBytes.size();
    AppendU16(HeaderBytes, 0);
    AppendU32(HeaderBytes, Normalized.Flags);
    AppendText32(HeaderBytes, Normalized.AssetId.ToString().View());
    AppendText16(HeaderBytes, Normalized.AssetType.View());
    AppendText16(HeaderBytes, Normalized.CodecId.View());
    AppendU32(HeaderBytes, Normalized.CodecVersion);
    AppendU32(HeaderBytes, Normalized.PayloadSchemaVersion);
    AppendU64(HeaderBytes, Normalized.BodyBytes);
    HeaderBytes.insert(
        HeaderBytes.end(),
        Normalized.BodyDigest.GetBytes().begin(),
        Normalized.BodyDigest.GetBytes().end());
    HeaderBytes.insert(
        HeaderBytes.end(),
        ReservedHeaderExtensions.begin(),
        ReservedHeaderExtensions.end());
    if (HeaderBytes.size() > Limits.MaxHeaderBytes ||
        HeaderBytes.size() > std::numeric_limits<Core::uint16>::max())
        return EAssetResult::CapacityExceeded;
    const Core::uint16 HeaderSize = static_cast<Core::uint16>(HeaderBytes.size());
    HeaderBytes[HeaderSizeOffset] = static_cast<Core::uint8>(HeaderSize);
    HeaderBytes[HeaderSizeOffset + 1] = static_cast<Core::uint8>(HeaderSize >> 8U);
    Core::uint64 Total = 0;
    if (!CheckedAdd(HeaderBytes.size(), Body.size(), Total) ||
        Total > Limits.MaxEnvelopeBytes)
        return EAssetResult::CapacityExceeded;
    OutBytes = std::move(HeaderBytes);
    OutBytes.insert(OutBytes.end(), Body.begin(), Body.end());
    if (OutEnvelope)
    {
        OutEnvelope->Header = Normalized;
        OutEnvelope->ReservedHeaderExtensions.assign(
            ReservedHeaderExtensions.begin(), ReservedHeaderExtensions.end());
        OutEnvelope->Body.assign(Body.begin(), Body.end());
        OutEnvelope->EnvelopeDigest = FAssetDigest::FromBytes(OutBytes);
    }
    return EAssetResult::Success;
}

namespace
{
EAssetResult ParseAssetCookedPayloadWithAuthority(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    bool bPreviouslyAuthenticated,
    bool bCopyBody,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope,
    std::span<const Core::uint8>* OutBodyView = nullptr)
{
    OutEnvelope = {};
    if (OutBodyView) *OutBodyView = {};
    if (Limits.Validate() != EAssetResult::Success ||
        Bytes.size() > Limits.MaxEnvelopeBytes || Bytes.size() < 72 ||
        !std::equal(Magic.begin(), Magic.end(), Bytes.begin()) ||
        (bPreviouslyAuthenticated && !ExpectedEnvelopeDigest.IsAvailable()))
        return EAssetResult::MalformedContainer;
    const FAssetDigest EnvelopeDigest = bPreviouslyAuthenticated
        ? ExpectedEnvelopeDigest
        : FAssetDigest::FromBytes(Bytes);
    if (!bPreviouslyAuthenticated && ExpectedEnvelopeDigest.IsAvailable() &&
        EnvelopeDigest != ExpectedEnvelopeDigest)
        return EAssetResult::CorruptPayload;
    FReader Reader(Bytes);
    if (!Reader.Skip(Magic.size())) return EAssetResult::MalformedContainer;
    FAssetCookedPayloadEnvelope Parsed;
    Core::uint16 HeaderBytes = 0;
    Core::FString AssetIdText;
    if (!Reader.ReadU16(Parsed.Header.ContainerVersion) ||
        !Reader.ReadU16(HeaderBytes) || !Reader.ReadU32(Parsed.Header.Flags) ||
        !Reader.ReadText32(AssetIdText) ||
        !Reader.ReadText16(Parsed.Header.AssetType) ||
        !Reader.ReadText16(Parsed.Header.CodecId) ||
        !Reader.ReadU32(Parsed.Header.CodecVersion) ||
        !Reader.ReadU32(Parsed.Header.PayloadSchemaVersion) ||
        !Reader.ReadU64(Parsed.Header.BodyBytes))
        return EAssetResult::MalformedContainer;
    if (Parsed.Header.ContainerVersion !=
        FAssetCookedPayloadHeader::CurrentContainerVersion)
        return EAssetResult::UnsupportedSchema;
    if (HeaderBytes < Reader.Offset() + 32 ||
        HeaderBytes > Limits.MaxHeaderBytes || HeaderBytes > Bytes.size())
        return EAssetResult::MalformedContainer;
    Core::TArray<Core::uint8> DigestBytes;
    if (!Reader.ReadBytes(32, DigestBytes)) return EAssetResult::MalformedContainer;
    const std::string DigestText = [&]() {
        constexpr char Hex[] = "0123456789abcdef";
        std::string Text(64, '0');
        for (Core::usize Index = 0; Index < DigestBytes.size(); ++Index)
        {
            Text[Index * 2] = Hex[DigestBytes[Index] >> 4U];
            Text[Index * 2 + 1] = Hex[DigestBytes[Index] & 0x0fU];
        }
        return Text;
    }();
    if (FAssetDigest::ParseLowerHex(
            Core::FString(DigestText), Parsed.Header.BodyDigest) !=
        EAssetResult::Success)
        return EAssetResult::MalformedContainer;
    const Core::usize ExtensionBytes = HeaderBytes - Reader.Offset();
    if (!Reader.ReadBytes(ExtensionBytes, Parsed.ReservedHeaderExtensions))
        return EAssetResult::MalformedContainer;
    if (Parsed.Header.BodyBytes == 0 ||
        Parsed.Header.BodyBytes > Limits.MaxBodyBytes ||
        Parsed.Header.BodyBytes > std::numeric_limits<Core::usize>::max() ||
        HeaderBytes + Parsed.Header.BodyBytes != Bytes.size())
        return EAssetResult::MalformedContainer;
    const Core::usize BodyOffset = Reader.Offset();
    const Core::usize BodyBytes = static_cast<Core::usize>(
        Parsed.Header.BodyBytes);
    if (bCopyBody)
    {
        if (!Reader.ReadBytes(BodyBytes, Parsed.Body))
            return EAssetResult::MalformedContainer;
    }
    else if (!Reader.Skip(BodyBytes))
        return EAssetResult::MalformedContainer;
    const EAssetResult IdResult = ParseAssetId(AssetIdText, Parsed.Header.AssetId);
    if (IdResult != EAssetResult::Success)
        return IdResult;
    const EAssetResult HeaderResult = Parsed.Header.Validate();
    if (HeaderResult != EAssetResult::Success)
        return HeaderResult;
    if (!ExpectedEnvelopeDigest.IsAvailable() &&
        FAssetDigest::FromBytes(Parsed.Body) != Parsed.Header.BodyDigest)
        return EAssetResult::CorruptPayload;
    Parsed.EnvelopeDigest = EnvelopeDigest;
    if (OutBodyView)
        *OutBodyView = Bytes.subspan(BodyOffset, BodyBytes);
    OutEnvelope = std::move(Parsed);
    return EAssetResult::Success;
}
} // namespace

EAssetResult ParseAssetCookedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope)
{
    return ParseAssetCookedPayloadWithAuthority(
        Bytes, {}, false, true, Limits, OutEnvelope);
}

EAssetResult ParseManifestAuthenticatedAssetCookedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope)
{
    return ParseAssetCookedPayloadWithAuthority(
        Bytes, ExpectedEnvelopeDigest, false, true, Limits, OutEnvelope);
}

EAssetResult ParsePreviouslyAuthenticatedAssetCookedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope)
{
    return ParseAssetCookedPayloadWithAuthority(
        Bytes, ExpectedEnvelopeDigest, true, true, Limits, OutEnvelope);
}

EAssetResult ParsePreviouslyAuthenticatedAssetCookedPayloadView(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope,
    std::span<const Core::uint8>& OutBody)
{
    return ParseAssetCookedPayloadWithAuthority(
        Bytes, ExpectedEnvelopeDigest, true, false, Limits, OutEnvelope,
        &OutBody);
}

} // namespace Stoner::Asset::Private
