#include "FMaterialShaderCookedCodec.h"

#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"
#include "Asset/FShaderAsset.h"
#include "Asset/FShaderPayloadAsset.h"
#include "Asset/FShaderNativeBindingEvidence.h"
#include "Asset/FShaderNativeLibraryEvidence.h"
#include "Asset/FShaderSourceAsset.h"
#include "FAssetCookedBinary.h"
#include "FMaterialShaderJsonCodec.h"

#include <memory>
#include <new>
#include <type_traits>

namespace Stoner::Asset::Private
{
namespace
{

void WriteOptionalDigest(
    FCookedBinaryWriter& Writer,
    const FAssetDigest& Digest)
{
    Writer.Bool(Digest.IsAvailable());
    if (Digest.IsAvailable()) Writer.Digest(Digest);
}

bool ReadOptionalDigest(
    FCookedBinaryReader& Reader,
    FAssetDigest& OutDigest)
{
    bool Present = false;
    if (!Reader.Bool(Present)) return false;
    OutDigest = {};
    return !Present || Reader.Digest(OutDigest);
}

void WriteVersion(FCookedBinaryWriter& Writer, const FAssetVersion& Version)
{
    WriteOptionalDigest(Writer, Version.SourceDigest);
    WriteOptionalDigest(Writer, Version.ContentDigest);
    WriteOptionalDigest(Writer, Version.CookDigest);
    Writer.Bool(Version.Producer.IsValid());
    if (Version.Producer.IsValid()) Writer.Text(Version.Producer.ToString());
    Writer.Bool(Version.ProducerVersion.IsValid());
    if (Version.ProducerVersion.IsValid())
        Writer.Text(Version.ProducerVersion.ToString());
    Writer.OptionalText(Version.TargetProfile);
}

bool ReadVersion(FCookedBinaryReader& Reader, FAssetVersion& Out)
{
    bool HasProducer = false;
    bool HasVersion = false;
    Core::FString Producer;
    Core::FString ProducerVersion;
    if (!ReadOptionalDigest(Reader, Out.SourceDigest) ||
        !ReadOptionalDigest(Reader, Out.ContentDigest) ||
        !ReadOptionalDigest(Reader, Out.CookDigest) ||
        !Reader.Bool(HasProducer) ||
        (HasProducer && !Reader.Text(Producer)) ||
        !Reader.Bool(HasVersion) ||
        (HasVersion && !Reader.Text(ProducerVersion)) ||
        !Reader.OptionalText(Out.TargetProfile))
        return false;
    if (HasProducer && FAssetParticipantId::Create(Producer, Out.Producer) !=
            EAssetResult::Success)
        return false;
    if (HasVersion && FAssetProducerVersion::Create(
            ProducerVersion, Out.ProducerVersion) != EAssetResult::Success)
        return false;
    return Out.Validate() == EAssetResult::Success;
}

void WritePermutation(
    FCookedBinaryWriter& Writer,
    const FShaderPermutationKey& Permutation)
{
    Writer.U32(static_cast<Core::uint32>(Permutation.Flags.size()));
    for (const auto& Flag : Permutation.Flags) Writer.Text(Flag);
}

bool ReadPermutation(
    FCookedBinaryReader& Reader,
    FShaderPermutationKey& Out)
{
    Core::uint32 Count = 0;
    if (!Reader.Count(Count)) return false;
    Out.Flags.reserve(Count);
    for (Core::uint32 Index = 0; Index < Count; ++Index)
    {
        Core::FString Flag;
        if (!Reader.Text(Flag)) return false;
        Out.Flags.push_back(std::move(Flag));
    }
    return true;
}

void WriteNativeBindingEvidence(
    FCookedBinaryWriter& Writer,
    const FShaderNativeBindingEvidence& Evidence)
{
    Writer.Text(Evidence.PolicyVersion);
    Writer.U32(static_cast<Core::uint32>(Evidence.Entries.size()));
    for (const auto& Entry : Evidence.Entries)
    {
        Writer.U8(static_cast<Core::uint8>(Entry.Stage));
        Writer.U32(Entry.SetIndex);
        Writer.U32(Entry.BindingIndex);
        Writer.U8(static_cast<Core::uint8>(Entry.DescriptorType));
        Writer.U32(Entry.ArrayElement);
        Writer.U8(static_cast<Core::uint8>(Entry.NativeClass));
        Writer.U32(Entry.NativeIndex);
    }
    Writer.U32(static_cast<Core::uint32>(Evidence.ReservedRanges.size()));
    for (const auto& Range : Evidence.ReservedRanges)
    {
        Writer.U8(static_cast<Core::uint8>(Range.Stage));
        Writer.U8(static_cast<Core::uint8>(Range.NativeClass));
        Writer.U32(Range.FirstIndex);
        Writer.U32(Range.Count);
        Writer.Text(Range.Purpose);
    }
    Writer.U32(static_cast<Core::uint32>(Evidence.LimitSnapshot.size()));
    for (const auto& Limit : Evidence.LimitSnapshot)
    {
        Writer.U8(static_cast<Core::uint8>(Limit.Stage));
        Writer.U8(static_cast<Core::uint8>(Limit.NativeClass));
        Writer.U32(Limit.MaxCount);
    }
    Writer.Digest(Evidence.CanonicalDigest);
}

bool ReadNativeBindingEvidence(
    FCookedBinaryReader& Reader,
    FShaderNativeBindingEvidence& Out)
{
    Out = {};
    Core::uint32 Count = 0;
    if (!Reader.Text(Out.PolicyVersion) || !Reader.Count(Count)) return false;
    Out.Entries.reserve(Count);
    for (Core::uint32 Index = 0; Index < Count; ++Index)
    {
        Core::uint8 Stage = 0;
        Core::uint8 DescriptorType = 0;
        Core::uint8 NativeClass = 0;
        FShaderNativeBindingEntry Entry;
        if (!Reader.U8(Stage) || !Reader.U32(Entry.SetIndex) ||
            !Reader.U32(Entry.BindingIndex) || !Reader.U8(DescriptorType) ||
            !Reader.U32(Entry.ArrayElement) || !Reader.U8(NativeClass) ||
            !Reader.U32(Entry.NativeIndex))
            return false;
        Entry.Stage = static_cast<EShaderStage>(Stage);
        Entry.DescriptorType = static_cast<EShaderResourceKind>(DescriptorType);
        Entry.NativeClass = static_cast<EShaderNativeResourceClass>(NativeClass);
        Out.Entries.push_back(Entry);
    }
    if (!Reader.Count(Count)) return false;
    Out.ReservedRanges.reserve(Count);
    for (Core::uint32 Index = 0; Index < Count; ++Index)
    {
        Core::uint8 Stage = 0;
        Core::uint8 NativeClass = 0;
        FShaderNativeReservedRange Range;
        if (!Reader.U8(Stage) || !Reader.U8(NativeClass) ||
            !Reader.U32(Range.FirstIndex) || !Reader.U32(Range.Count) ||
            !Reader.Text(Range.Purpose))
            return false;
        Range.Stage = static_cast<EShaderStage>(Stage);
        Range.NativeClass =
            static_cast<EShaderNativeResourceClass>(NativeClass);
        Out.ReservedRanges.push_back(std::move(Range));
    }
    if (!Reader.Count(Count)) return false;
    Out.LimitSnapshot.reserve(Count);
    for (Core::uint32 Index = 0; Index < Count; ++Index)
    {
        Core::uint8 Stage = 0;
        Core::uint8 NativeClass = 0;
        FShaderNativeBindingLimit Limit;
        if (!Reader.U8(Stage) || !Reader.U8(NativeClass) ||
            !Reader.U32(Limit.MaxCount))
            return false;
        Limit.Stage = static_cast<EShaderStage>(Stage);
        Limit.NativeClass =
            static_cast<EShaderNativeResourceClass>(NativeClass);
        Out.LimitSnapshot.push_back(Limit);
    }
    return Reader.Digest(Out.CanonicalDigest) &&
        Out.Validate() == EAssetResult::Success;
}

void WriteNativeLibraryEvidence(
    FCookedBinaryWriter& Writer,
    const FShaderNativeLibraryEvidence& Evidence)
{
    Writer.Digest(Evidence.DerivationEvidenceDigest);
    Writer.Text(Evidence.TargetProfile);
    Writer.Text(Evidence.Architecture);
    Writer.Text(Evidence.Compiler);
    Writer.Text(Evidence.XcodeBuild);
    Writer.Text(Evidence.Sdk);
    Writer.Text(Evidence.DeploymentTarget);
    Writer.Text(Evidence.LanguageVersion);
    Writer.Digest(Evidence.ArgumentDigest);
    Writer.Digest(Evidence.LibraryDigest);
    Writer.U64(Evidence.SizeBytes);
    Writer.Text(Evidence.Finalizer.ToString());
    Writer.Text(Evidence.FinalizerVersion.ToString());
    Writer.Digest(Evidence.CanonicalDigest);
}

bool ReadNativeLibraryEvidence(
    FCookedBinaryReader& Reader,
    FShaderNativeLibraryEvidence& Out)
{
    Out = {};
    Core::FString Finalizer;
    Core::FString FinalizerVersion;
    if (!Reader.Digest(Out.DerivationEvidenceDigest) ||
        !Reader.Text(Out.TargetProfile) || !Reader.Text(Out.Architecture) ||
        !Reader.Text(Out.Compiler) || !Reader.Text(Out.XcodeBuild) ||
        !Reader.Text(Out.Sdk) || !Reader.Text(Out.DeploymentTarget) ||
        !Reader.Text(Out.LanguageVersion) ||
        !Reader.Digest(Out.ArgumentDigest) ||
        !Reader.Digest(Out.LibraryDigest) || !Reader.U64(Out.SizeBytes) ||
        !Reader.Text(Finalizer) || !Reader.Text(FinalizerVersion) ||
        !Reader.Digest(Out.CanonicalDigest) ||
        FAssetParticipantId::Create(Finalizer, Out.Finalizer) !=
            EAssetResult::Success ||
        FAssetProducerVersion::Create(
            FinalizerVersion, Out.FinalizerVersion) != EAssetResult::Success)
        return false;
    return Out.Validate() == EAssetResult::Success;
}

FAssetCookedPayloadHeader Header(const FAssetId& Id, const char* Codec)
{
    FAssetCookedPayloadHeader Value;
    Value.AssetId = Id;
    Value.AssetType = Id.GetAssetType();
    Value.CodecId = Core::FString(Codec);
    Value.CodecVersion = 1;
    Value.PayloadSchemaVersion = 1;
    return Value;
}

template <typename T>
EAssetResult EncodeDefinition(
    const T& Asset,
    const FAssetId& Id,
    const char* Codec,
    Core::uint64 MaximumBytes,
    FAssetCookedPayloadHeader& OutHeader,
    Core::TArray<Core::uint8>& OutBody)
{
    FMaterialShaderDefinition Definition;
    if constexpr (std::is_same_v<T, FShaderAsset>)
        Definition.Kind = EMaterialShaderDefinitionKind::Shader;
    else if constexpr (std::is_same_v<T, FMaterialAsset>)
        Definition.Kind = EMaterialShaderDefinitionKind::Material;
    else
        Definition.Kind = EMaterialShaderDefinitionKind::MaterialInstance;
    Definition.Value = Asset.GetDesc();
    Core::FString Canonical;
    const EAssetResult CanonicalResult = WriteMaterialShaderDefinition(
        Definition, Canonical, nullptr);
    if (CanonicalResult != EAssetResult::Success) return CanonicalResult;
    FCookedBinaryWriter Writer(MaximumBytes);
    Writer.Text(Canonical);
    OutBody = Writer.Take();
    if (OutBody.empty()) return EAssetResult::InvalidDefinition;
    OutHeader = Header(Id, Codec);
    return EAssetResult::Success;
}

template <typename T, typename TDesc>
EAssetResult PublishDefinition(
    FMaterialShaderDefinition& Definition,
    const FAssetId& ExpectedId,
    Core::TSharedPtr<const FAssetPayload>& Out)
{
    auto* Desc = std::get_if<TDesc>(&Definition.Value);
    if (!Desc || Desc->Id != ExpectedId)
        return EAssetResult::TypeMismatch;
    T Asset;
    const EAssetResult Result = T::CreateValidated(
        std::move(*Desc), Asset, nullptr);
    if (Result != EAssetResult::Success) return Result;
    Out = Core::MakeShared<T>(std::move(Asset));
    return EAssetResult::Success;
}

} // namespace

EAssetResult EncodeMaterialShaderCookedBody(
    const FAssetPayload& Payload,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadHeader& OutHeader,
    Core::TArray<Core::uint8>& OutBody)
{
    OutHeader = {};
    OutBody.clear();
    if (Limits.Validate() != EAssetResult::Success)
        return EAssetResult::InvalidInput;
    try
    {
        if (const auto* Shader = dynamic_cast<const FShaderAsset*>(&Payload))
            return EncodeDefinition(
                *Shader, Shader->GetDesc().Id, "stoner.shader-program",
                Limits.MaxBodyBytes, OutHeader, OutBody);
        if (const auto* Material = dynamic_cast<const FMaterialAsset*>(&Payload))
            return EncodeDefinition(
                *Material, Material->GetDesc().Id, "stoner.material",
                Limits.MaxBodyBytes, OutHeader, OutBody);
        if (const auto* Instance =
                dynamic_cast<const FMaterialInstanceAsset*>(&Payload))
            return EncodeDefinition(
                *Instance, Instance->GetDesc().Id, "stoner.material-instance",
                Limits.MaxBodyBytes, OutHeader, OutBody);

        FCookedBinaryWriter Writer(Limits.MaxBodyBytes);
        if (const auto* Source = dynamic_cast<const FShaderSourceAsset*>(&Payload))
        {
            Writer.AssetId(Source->GetId());
            WriteVersion(Writer, Source->GetVersion());
            Writer.U8(static_cast<Core::uint8>(Source->GetLanguage()));
            Writer.Bytes(Source->GetBytes());
            OutHeader = Header(Source->GetId(), "stoner.shader-source");
        }
        else if (const auto* ShaderPayload =
                     dynamic_cast<const FShaderPayloadAsset*>(&Payload))
        {
            Writer.AssetId(ShaderPayload->GetId());
            WriteVersion(Writer, ShaderPayload->GetVersion());
            Writer.U8(static_cast<Core::uint8>(ShaderPayload->GetBackend()));
            Writer.Text(ShaderPayload->GetProfile());
            Writer.U8(static_cast<Core::uint8>(ShaderPayload->GetFormat()));
            Writer.U8(static_cast<Core::uint8>(ShaderPayload->GetStage()));
            Writer.Text(ShaderPayload->GetEntryPoint());
            WritePermutation(Writer, ShaderPayload->GetPermutation());
            Writer.Bytes(ShaderPayload->GetBytes());
            OutHeader = Header(
                ShaderPayload->GetId(), "stoner.shader-payload");
            if (ShaderPayload->GetFormat() == EShaderPayloadFormat::MetalLibrary)
            {
                const auto* Evidence =
                    ShaderPayload->GetNativeBindingEvidence();
                const auto* LibraryEvidence =
                    ShaderPayload->GetNativeLibraryEvidence();
                if (!Evidence || !LibraryEvidence ||
                    Evidence->Validate() != EAssetResult::Success ||
                    LibraryEvidence->Validate() != EAssetResult::Success)
                    return EAssetResult::InvalidInput;
                WriteNativeBindingEvidence(Writer, *Evidence);
                WriteNativeLibraryEvidence(Writer, *LibraryEvidence);
                OutHeader.CodecVersion = 2;
                OutHeader.PayloadSchemaVersion = 2;
            }
        }
        else return EAssetResult::TypeMismatch;
        OutBody = Writer.Take();
        if (OutBody.empty())
        {
            OutHeader = {};
            return EAssetResult::CapacityExceeded;
        }
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        OutHeader = {};
        OutBody.clear();
        return EAssetResult::CapacityExceeded;
    }
}

EAssetResult DecodeMaterialShaderCookedBody(
    const FAssetCookedPayloadHeader& HeaderValue,
    std::span<const Core::uint8> Body,
    Core::TSharedPtr<const FAssetPayload>& OutPayload)
{
    OutPayload.reset();
    try
    {
        FCookedBinaryReader Reader(Body);
        if (HeaderValue.CodecId == Core::FString("stoner.shader-program") ||
            HeaderValue.CodecId == Core::FString("stoner.material") ||
            HeaderValue.CodecId == Core::FString("stoner.material-instance"))
        {
            Core::FString Canonical;
            if (!Reader.Text(Canonical) || !Reader.AtEnd())
                return EAssetResult::CorruptPayload;
            FMaterialShaderDefinition Definition;
            const EAssetResult Parse = ParseMaterialShaderDefinition(
                std::span<const Core::uint8>(
                    reinterpret_cast<const Core::uint8*>(
                        Canonical.View().data()),
                    Canonical.Len()),
                {}, Definition, nullptr);
            if (Parse != EAssetResult::Success) return Parse;
            if (HeaderValue.CodecId == Core::FString("stoner.shader-program"))
                return PublishDefinition<FShaderAsset, FShaderAssetDesc>(
                    Definition, HeaderValue.AssetId, OutPayload);
            if (HeaderValue.CodecId == Core::FString("stoner.material"))
                return PublishDefinition<FMaterialAsset, FMaterialAssetDesc>(
                    Definition, HeaderValue.AssetId, OutPayload);
            return PublishDefinition<
                FMaterialInstanceAsset, FMaterialInstanceAssetDesc>(
                    Definition, HeaderValue.AssetId, OutPayload);
        }

        FAssetId Id;
        FAssetVersion Version;
        if (!Reader.AssetId(Id) || !ReadVersion(Reader, Version) ||
            Id != HeaderValue.AssetId)
            return EAssetResult::CorruptPayload;
        if (HeaderValue.CodecId == Core::FString("stoner.shader-source"))
        {
            Core::uint8 Language = 0;
            Core::TArray<Core::uint8> Bytes;
            if (!Reader.U8(Language) || !Reader.Bytes(Bytes) || !Reader.AtEnd())
                return EAssetResult::CorruptPayload;
            FShaderSourceAsset Asset;
            const EAssetResult Result = FShaderSourceAsset::Create(
                std::move(Id), std::move(Version),
                static_cast<EShaderSourceLanguage>(Language),
                std::move(Bytes), Asset);
            if (Result != EAssetResult::Success) return Result;
            OutPayload = Core::MakeShared<FShaderSourceAsset>(std::move(Asset));
            return EAssetResult::Success;
        }
        if (HeaderValue.CodecId == Core::FString("stoner.shader-payload"))
        {
            Core::uint8 Backend = 0;
            Core::uint8 Format = 0;
            Core::uint8 Stage = 0;
            Core::FString Profile;
            Core::FString Entry;
            FShaderPermutationKey Permutation;
            Core::TArray<Core::uint8> Bytes;
            if (!Reader.U8(Backend) || !Reader.Text(Profile) ||
                !Reader.U8(Format) || !Reader.U8(Stage) ||
                !Reader.Text(Entry) || !ReadPermutation(Reader, Permutation) ||
                !Reader.Bytes(Bytes))
                return EAssetResult::CorruptPayload;
            FShaderPayloadAsset Asset;
            EAssetResult Result = EAssetResult::CorruptPayload;
            if (HeaderValue.PayloadSchemaVersion == 1)
            {
                if (!Reader.AtEnd()) return EAssetResult::CorruptPayload;
                Result = FShaderPayloadAsset::Create(
                    std::move(Id), std::move(Version),
                    static_cast<EShaderBackendFamily>(Backend),
                    std::move(Profile), static_cast<EShaderPayloadFormat>(Format),
                    static_cast<EShaderStage>(Stage), std::move(Entry),
                    std::move(Permutation), std::move(Bytes), Asset);
            }
            else if (HeaderValue.PayloadSchemaVersion == 2)
            {
                FShaderNativeBindingEvidence Evidence;
                FShaderNativeLibraryEvidence LibraryEvidence;
                if (!ReadNativeBindingEvidence(Reader, Evidence) ||
                    !ReadNativeLibraryEvidence(Reader, LibraryEvidence) ||
                    !Reader.AtEnd())
                    return EAssetResult::CorruptPayload;
                Result = FShaderPayloadAsset::CreateWithNativeEvidence(
                    std::move(Id), std::move(Version),
                    static_cast<EShaderBackendFamily>(Backend),
                    std::move(Profile), static_cast<EShaderPayloadFormat>(Format),
                    static_cast<EShaderStage>(Stage), std::move(Entry),
                    std::move(Permutation), std::move(Bytes),
                    std::move(Evidence), std::move(LibraryEvidence), Asset);
            }
            if (Result != EAssetResult::Success) return Result;
            OutPayload = Core::MakeShared<FShaderPayloadAsset>(std::move(Asset));
            return EAssetResult::Success;
        }
        return EAssetResult::Unsupported;
    }
    catch (const std::bad_alloc&)
    {
        OutPayload.reset();
        return EAssetResult::CapacityExceeded;
    }
}

} // namespace Stoner::Asset::Private
