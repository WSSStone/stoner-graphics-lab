#include "FAssetDerivedDataEntryCodec.h"

#include "Asset/FAssetCookContractCodec.h"

#include "../../../ThirdParty/yyjson/yyjson.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Stoner::Asset::Private
{
namespace
{

void Escape(std::string_view Text, std::string& Out)
{
    Out.push_back('"');
    constexpr char Hex[] = "0123456789abcdef";
    for (const unsigned char Character : Text)
    {
        switch (Character)
        {
        case '"': Out += "\\\""; break;
        case '\\': Out += "\\\\"; break;
        case '\b': Out += "\\b"; break;
        case '\f': Out += "\\f"; break;
        case '\n': Out += "\\n"; break;
        case '\r': Out += "\\r"; break;
        case '\t': Out += "\\t"; break;
        default:
            if (Character < 0x20)
            {
                Out += "\\u00";
                Out.push_back(Hex[Character >> 4U]);
                Out.push_back(Hex[Character & 0x0fU]);
            }
            else Out.push_back(static_cast<char>(Character));
        }
    }
    Out.push_back('"');
}

void Key(std::string& Out, const char* Name)
{
    Escape(Name, Out);
    Out.push_back(':');
}

void DigestText(const FAssetDigest& Digest, std::string& Out)
{
    Escape(Digest.IsAvailable() ? Digest.ToLowerHex().View() : std::string_view{}, Out);
}

const char* RoleText(EAssetDependencyRole Role)
{
    switch (Role)
    {
    case EAssetDependencyRole::Source: return "source";
    case EAssetDependencyRole::Build: return "build";
    case EAssetDependencyRole::Runtime: return "runtime";
    }
    return "invalid";
}

void VersionJson(const FAssetVersion& Version, std::string& Out)
{
    Out.push_back('{');
    Key(Out, "sourceDigest"); DigestText(Version.SourceDigest, Out); Out.push_back(',');
    Key(Out, "contentDigest"); DigestText(Version.ContentDigest, Out); Out.push_back(',');
    Key(Out, "cookDigest"); DigestText(Version.CookDigest, Out); Out.push_back(',');
    Key(Out, "producer"); Escape(Version.Producer.IsValid() ? Version.Producer.ToString().View() : std::string_view{}, Out); Out.push_back(',');
    Key(Out, "producerVersion"); Escape(Version.ProducerVersion.IsValid() ? Version.ProducerVersion.ToString().View() : std::string_view{}, Out); Out.push_back(',');
    Key(Out, "targetProfile"); Escape(Version.TargetProfile ? Version.TargetProfile->View() : std::string_view{}, Out);
    Out.push_back('}');
}

std::string EntryJson(const FAssetDerivedDataEntry& Entry)
{
    const auto& Evidence = Entry.Evidence;
    std::string Out;
    Out.reserve(4096);
    Out.push_back('{');
    Key(Out, "schema"); Escape(Entry.Schema.View(), Out); Out.push_back(',');
    Key(Out, "schemaVersion"); Out += std::to_string(Entry.SchemaVersion); Out.push_back(',');
    Key(Out, "derivedKey"); Escape(Entry.DerivedKey.ToString().View(), Out); Out.push_back(',');
    Key(Out, "assetId"); Escape(Entry.AssetId.ToString().View(), Out); Out.push_back(',');
    Key(Out, "evidence"); Out.push_back('{');
    Key(Out, "keyFormatVersion"); Out += std::to_string(Evidence.KeyFormatVersion); Out.push_back(',');
    Key(Out, "assetId"); Escape(Evidence.AssetId.ToString().View(), Out); Out.push_back(',');
    Key(Out, "sourceVersion"); DigestText(Evidence.SourceVersion, Out); Out.push_back(',');
    Key(Out, "sources"); Out.push_back('[');
    for (Core::usize Index = 0; Index < Evidence.SourceManifest.size(); ++Index)
    {
        if (Index) Out.push_back(',');
        Out.push_back('{'); Key(Out, "locator"); Escape(Evidence.SourceManifest[Index].Locator.ToString().View(), Out); Out.push_back(',');
        Key(Out, "version"); DigestText(Evidence.SourceManifest[Index].Version, Out); Out.push_back('}');
    }
    Out += "],";
    Key(Out, "dependencies"); Out.push_back('[');
    for (Core::usize Index = 0; Index < Evidence.Dependencies.size(); ++Index)
    {
        if (Index) Out.push_back(',');
        const auto& Dependency = Evidence.Dependencies[Index];
        Out.push_back('{'); Key(Out, "assetId"); Escape(Dependency.Id.ToString().View(), Out); Out.push_back(',');
        Key(Out, "role"); Escape(RoleText(Dependency.Role), Out); Out.push_back(',');
        Key(Out, "version"); VersionJson(Dependency.Version, Out); Out.push_back('}');
    }
    Out += "],";
    Key(Out, "importerId"); Escape(Evidence.ImporterId.ToString().View(), Out); Out.push_back(',');
    Key(Out, "importerVersion"); Escape(Evidence.ImporterVersion.ToString().View(), Out); Out.push_back(',');
    Key(Out, "cookerId"); Escape(Evidence.CookerId.ToString().View(), Out); Out.push_back(',');
    Key(Out, "cookerVersion"); Escape(Evidence.CookerVersion.ToString().View(), Out); Out.push_back(',');
    Key(Out, "codecId"); Escape(Evidence.CodecId.ToString().View(), Out); Out.push_back(',');
    Key(Out, "codecVersion"); Escape(Evidence.CodecVersion.ToString().View(), Out); Out.push_back(',');
    Key(Out, "payloadSchemaVersion"); Out += std::to_string(Evidence.PayloadSchemaVersion); Out.push_back(',');
    Key(Out, "effectiveSettingsDigest"); DigestText(Evidence.EffectiveSettingsDigest, Out); Out.push_back(',');
    Key(Out, "relevantProfileDigest"); DigestText(Evidence.RelevantProfileDigest, Out);
    Out += "},";
    Key(Out, "codecId"); Escape(Entry.CodecId.ToString().View(), Out); Out.push_back(',');
    Key(Out, "codecVersion"); Escape(Entry.CodecVersion.ToString().View(), Out); Out.push_back(',');
    Key(Out, "payloadSchemaVersion"); Out += std::to_string(Entry.PayloadSchemaVersion); Out.push_back(',');
    Key(Out, "relevantProfileDigest"); DigestText(Entry.RelevantProfileDigest, Out); Out.push_back(',');
    Key(Out, "payloadLocator"); Escape(Entry.PayloadLocator.View(), Out); Out.push_back(',');
    Key(Out, "payloadBytes"); Out += std::to_string(Entry.PayloadBytes); Out.push_back(',');
    Key(Out, "envelopeDigest"); DigestText(Entry.EnvelopeDigest, Out); Out.push_back(',');
    Key(Out, "requiredExtensions"); Out.push_back('[');
    for (Core::usize Index = 0; Index < Entry.RequiredExtensions.size(); ++Index)
    {
        if (Index) Out.push_back(',');
        Escape(Entry.RequiredExtensions[Index].View(), Out);
    }
    Out += "]}\n";
    return Out;
}

bool Closed(yyjson_val* Object, std::initializer_list<std::string_view> Allowed)
{
    if (!yyjson_is_obj(Object)) return false;
    std::set<std::string_view> Seen;
    yyjson_obj_iter Iterator = yyjson_obj_iter_with(Object);
    yyjson_val* KeyValue = nullptr;
    while ((KeyValue = yyjson_obj_iter_next(&Iterator)) != nullptr)
    {
        const std::string_view Name(yyjson_get_str(KeyValue), yyjson_get_len(KeyValue));
        if (std::find(Allowed.begin(), Allowed.end(), Name) == Allowed.end() ||
            !Seen.insert(Name).second) return false;
    }
    return Seen.size() == Allowed.size();
}

bool Text(yyjson_val* Object, const char* Name, Core::FString& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_str(Value) || yyjson_get_len(Value) > 4096) return false;
    Out = Core::FString(std::string(yyjson_get_str(Value), yyjson_get_len(Value)));
    return true;
}

template <typename T>
bool Unsigned(yyjson_val* Object, const char* Name, T& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_raw(Value)) return false;
    const char* Begin = yyjson_get_raw(Value);
    const char* End = Begin + yyjson_get_len(Value);
    T Parsed = 0;
    const auto [Position, Error] = std::from_chars(Begin, End, Parsed);
    if (Error != std::errc{} || Position != End) return false;
    Out = Parsed;
    return true;
}

bool Digest(yyjson_val* Object, const char* Name, FAssetDigest& Out, bool Optional = false)
{
    Core::FString Value;
    if (!Text(Object, Name, Value)) return false;
    if (Optional && Value.IsEmpty()) { Out = {}; return true; }
    return FAssetDigest::ParseLowerHex(Value, Out) == EAssetResult::Success;
}

bool AssetId(const Core::FString& TextValue, FAssetId& Out)
{
    const auto Value = TextValue.View();
    const auto Colon = Value.find(':');
    const auto Hash = Value.find('#', Colon == std::string_view::npos ? 0 : Colon + 1);
    if (Colon == std::string_view::npos || Colon == 0) return false;
    std::optional<Core::FString> Subresource;
    if (Hash != std::string_view::npos) Subresource = Core::FString(Value.substr(Hash + 1));
    return FAssetId::Create(
        Core::FString(Value.substr(0, Colon)),
        Core::FString(Value.substr(Colon + 1, Hash == std::string_view::npos ? std::string_view::npos : Hash - Colon - 1)),
        Subresource, Out) == EAssetResult::Success && Out.ToString() == TextValue;
}

bool Locator(const Core::FString& TextValue, FAssetSourceLocator& Out)
{
    const auto Value = TextValue.View();
    const auto Colon = Value.find(':');
    return Colon != std::string_view::npos && Colon > 0 &&
        FAssetSourceLocator::Create(Core::FString(Value.substr(0, Colon)),
            Core::FString(Value.substr(Colon + 1)), Out) == EAssetResult::Success &&
        Out.ToString() == TextValue;
}

bool ParseVersion(yyjson_val* Object, FAssetVersion& Out)
{
    if (!Closed(Object, {"sourceDigest", "contentDigest", "cookDigest", "producer", "producerVersion", "targetProfile"}) ||
        !Digest(Object, "sourceDigest", Out.SourceDigest, true) ||
        !Digest(Object, "contentDigest", Out.ContentDigest, true) ||
        !Digest(Object, "cookDigest", Out.CookDigest, true)) return false;
    Core::FString Producer, ProducerVersion, Target;
    if (!Text(Object, "producer", Producer) || !Text(Object, "producerVersion", ProducerVersion) || !Text(Object, "targetProfile", Target)) return false;
    if (!Producer.IsEmpty() && FAssetParticipantId::Create(Producer, Out.Producer) != EAssetResult::Success) return false;
    if (!ProducerVersion.IsEmpty() && FAssetProducerVersion::Create(ProducerVersion, Out.ProducerVersion) != EAssetResult::Success) return false;
    if (!Target.IsEmpty()) Out.TargetProfile = Target;
    return Out.Validate() == EAssetResult::Success;
}

bool ParseEvidence(yyjson_val* Object, const FAssetDerivedDataEntryLimits& Limits, FAssetDerivedKeyEvidence& Out)
{
    if (!Closed(Object, {"keyFormatVersion", "assetId", "sourceVersion", "sources", "dependencies", "importerId", "importerVersion", "cookerId", "cookerVersion", "codecId", "codecVersion", "payloadSchemaVersion", "effectiveSettingsDigest", "relevantProfileDigest"}) ||
        !Unsigned(Object, "keyFormatVersion", Out.KeyFormatVersion) ||
        !Unsigned(Object, "payloadSchemaVersion", Out.PayloadSchemaVersion) ||
        !Digest(Object, "sourceVersion", Out.SourceVersion) ||
        !Digest(Object, "effectiveSettingsDigest", Out.EffectiveSettingsDigest) ||
        !Digest(Object, "relevantProfileDigest", Out.RelevantProfileDigest)) return false;
    Core::FString Id, ImporterId, ImporterVersion, CookerId, CookerVersion, CodecId, CodecVersion;
    if (!Text(Object, "assetId", Id) || !AssetId(Id, Out.AssetId) ||
        !Text(Object, "importerId", ImporterId) || FAssetParticipantId::Create(ImporterId, Out.ImporterId) != EAssetResult::Success ||
        !Text(Object, "importerVersion", ImporterVersion) || FAssetProducerVersion::Create(ImporterVersion, Out.ImporterVersion) != EAssetResult::Success ||
        !Text(Object, "cookerId", CookerId) || FAssetParticipantId::Create(CookerId, Out.CookerId) != EAssetResult::Success ||
        !Text(Object, "cookerVersion", CookerVersion) || FAssetProducerVersion::Create(CookerVersion, Out.CookerVersion) != EAssetResult::Success ||
        !Text(Object, "codecId", CodecId) || FAssetParticipantId::Create(CodecId, Out.CodecId) != EAssetResult::Success ||
        !Text(Object, "codecVersion", CodecVersion) || FAssetProducerVersion::Create(CodecVersion, Out.CodecVersion) != EAssetResult::Success) return false;
    yyjson_val* Sources = yyjson_obj_get(Object, "sources");
    yyjson_val* Dependencies = yyjson_obj_get(Object, "dependencies");
    if (!yyjson_is_arr(Sources) || yyjson_arr_size(Sources) > Limits.MaxSources ||
        !yyjson_is_arr(Dependencies) || yyjson_arr_size(Dependencies) > Limits.MaxDependencies) return false;
    std::size_t Index = 0, Count = 0; yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Sources, Index, Count, Value)
    {
        Core::FString Location;
        FAssetDerivedSourceEvidence Source;
        if (!Closed(Value, {"locator", "version"}) || !Text(Value, "locator", Location) ||
            !Locator(Location, Source.Locator) || !Digest(Value, "version", Source.Version)) return false;
        Out.SourceManifest.push_back(std::move(Source));
    }
    yyjson_arr_foreach(Dependencies, Index, Count, Value)
    {
        Core::FString DependencyId, Role;
        FAssetDerivedDependencyEvidence Dependency;
        if (!Closed(Value, {"assetId", "role", "version"}) || !Text(Value, "assetId", DependencyId) ||
            !AssetId(DependencyId, Dependency.Id) || !Text(Value, "role", Role) ||
            !ParseVersion(yyjson_obj_get(Value, "version"), Dependency.Version)) return false;
        if (Role == Core::FString("source")) Dependency.Role = EAssetDependencyRole::Source;
        else if (Role == Core::FString("build")) Dependency.Role = EAssetDependencyRole::Build;
        else if (Role == Core::FString("runtime")) Dependency.Role = EAssetDependencyRole::Runtime;
        else return false;
        Out.Dependencies.push_back(std::move(Dependency));
    }
    return Out.Validate() == EAssetResult::Success;
}

} // namespace

EAssetResult WriteAssetDerivedDataEntry(
    const FAssetDerivedDataEntry& Entry,
    Core::FString& OutCanonical)
{
    OutCanonical = {};
    FAssetDerivedKey Recomputed;
    if (Entry.Validate() != EAssetResult::Success ||
        FAssetCookContractCodec::BuildDerivedKey(Entry.Evidence, Recomputed) != EAssetResult::Success ||
        Recomputed != Entry.DerivedKey) return EAssetResult::InvalidInput;
    OutCanonical = Core::FString(EntryJson(Entry));
    return EAssetResult::Success;
}

EAssetResult ParseAssetDerivedDataEntry(
    std::span<const Core::uint8> Bytes,
    const FAssetDerivedDataEntryLimits& Limits,
    FAssetDerivedDataEntry& OutEntry)
{
    OutEntry = {};
    if (Bytes.empty() || Bytes.size() > Limits.MaxMetadataBytes ||
        Limits.MaxMetadataBytes == 0 || Limits.MaxSources == 0 ||
        Limits.MaxDependencies == 0 || Limits.MaxRequiredExtensions == 0)
        return EAssetResult::DefinitionLimitExceeded;
    constexpr yyjson_read_flag Flags = YYJSON_READ_NUMBER_AS_RAW;
    const std::size_t Required = yyjson_read_max_memory_usage(Bytes.size(), Flags);
    if (Required == 0) return EAssetResult::MalformedSource;
    std::vector<unsigned char> Pool(Required);
    yyjson_alc Allocator{};
    if (!yyjson_alc_pool_init(&Allocator, Pool.data(), Pool.size())) return EAssetResult::ProcessingFailure;
    yyjson_doc* Document = yyjson_read_opts(
        const_cast<char*>(reinterpret_cast<const char*>(Bytes.data())),
        Bytes.size(), Flags, &Allocator, nullptr);
    if (!Document) return EAssetResult::MalformedSource;
    yyjson_val* Root = yyjson_doc_get_root(Document);
    FAssetDerivedDataEntry Parsed;
    Core::FString DerivedKey, AssetIdText, CodecId, CodecVersion;
    bool Valid = Closed(Root, {"schema", "schemaVersion", "derivedKey", "assetId", "evidence", "codecId", "codecVersion", "payloadSchemaVersion", "relevantProfileDigest", "payloadLocator", "payloadBytes", "envelopeDigest", "requiredExtensions"}) &&
        Text(Root, "schema", Parsed.Schema) && Unsigned(Root, "schemaVersion", Parsed.SchemaVersion) &&
        Text(Root, "derivedKey", DerivedKey) && FAssetDerivedKey::ParseLowerHex(DerivedKey, Parsed.DerivedKey) == EAssetResult::Success &&
        Text(Root, "assetId", AssetIdText) && AssetId(AssetIdText, Parsed.AssetId) &&
        ParseEvidence(yyjson_obj_get(Root, "evidence"), Limits, Parsed.Evidence) &&
        Text(Root, "codecId", CodecId) && FAssetParticipantId::Create(CodecId, Parsed.CodecId) == EAssetResult::Success &&
        Text(Root, "codecVersion", CodecVersion) && FAssetProducerVersion::Create(CodecVersion, Parsed.CodecVersion) == EAssetResult::Success &&
        Unsigned(Root, "payloadSchemaVersion", Parsed.PayloadSchemaVersion) &&
        Digest(Root, "relevantProfileDigest", Parsed.RelevantProfileDigest) &&
        Text(Root, "payloadLocator", Parsed.PayloadLocator) && Unsigned(Root, "payloadBytes", Parsed.PayloadBytes) &&
        Digest(Root, "envelopeDigest", Parsed.EnvelopeDigest);
    yyjson_val* Extensions = yyjson_obj_get(Root, "requiredExtensions");
    if (Valid && yyjson_is_arr(Extensions) && yyjson_arr_size(Extensions) <= Limits.MaxRequiredExtensions)
    {
        std::size_t Index = 0, Count = 0; yyjson_val* Value = nullptr;
        yyjson_arr_foreach(Extensions, Index, Count, Value)
        {
            if (!yyjson_is_str(Value) || yyjson_get_len(Value) == 0 || yyjson_get_len(Value) > 127) { Valid = false; break; }
            Parsed.RequiredExtensions.emplace_back(std::string(yyjson_get_str(Value), yyjson_get_len(Value)));
        }
    }
    else Valid = false;
    Core::FString Canonical;
    if (Valid) Valid = WriteAssetDerivedDataEntry(Parsed, Canonical) == EAssetResult::Success &&
        Canonical.Len() == Bytes.size() && std::equal(Bytes.begin(), Bytes.end(), Canonical.View().begin());
    yyjson_doc_free(Document);
    if (!Valid) return EAssetResult::MalformedSource;
    OutEntry = std::move(Parsed);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
