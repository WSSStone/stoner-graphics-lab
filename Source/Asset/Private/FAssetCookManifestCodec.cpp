#include "FAssetCookManifestCodec.h"

#include "FAssetTargetProfileCodec.h"

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

using FAllowed = std::initializer_list<std::string_view>;

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

void Indent(std::string& Out, int Depth)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
}

void Key(std::string& Out, int Depth, const char* Name)
{
    Indent(Out, Depth);
    Escape(Name, Out);
    Out += ": ";
}

void StringArray(std::string& Out, const Core::TArray<Core::FString>& Values)
{
    Out.push_back('[');
    for (Core::usize Index = 0; Index < Values.size(); ++Index)
    {
        if (Index) Out += ", ";
        Escape(Values[Index].View(), Out);
    }
    Out.push_back(']');
}

void AssetIdArray(std::string& Out, const Core::TArray<FAssetId>& Values)
{
    Out.push_back('[');
    for (Core::usize Index = 0; Index < Values.size(); ++Index)
    {
        if (Index) Out += ", ";
        Escape(Values[Index].ToString().View(), Out);
    }
    Out.push_back(']');
}

void ParticipantJson(
    std::string& Out,
    const FAssetCookManifestParticipant& Participant,
    int Depth)
{
    Out += "{\n";
    Key(Out, Depth + 1, "id"); Escape(Participant.Id.ToString().View(), Out); Out += ",\n";
    Key(Out, Depth + 1, "version"); Escape(Participant.Version.ToString().View(), Out); Out += "\n";
    Indent(Out, Depth); Out += "}";
}

void SelectionJson(std::string& Out, const FAssetCookSelection& Selection, int Depth)
{
    Out += "{\n";
    Key(Out, Depth + 1, "mode"); Escape(Selection.Mode == EAssetCookSelectionMode::ExplicitRoots ? "explicit-roots" : "cook-all", Out); Out += ",\n";
    Key(Out, Depth + 1, "roots"); AssetIdArray(Out, Selection.Roots); Out += ",\n";
    Key(Out, Depth + 1, "sourceScopes"); StringArray(Out, Selection.SourceScopes); Out += ",\n";
    Key(Out, Depth + 1, "discoveryRulesVersion"); Escape(Selection.DiscoveryRulesVersion.ToLowerHex().View(), Out); Out += "\n";
    Indent(Out, Depth); Out += "}";
}

void SourceRecordsJson(
    std::string& Out,
    const Core::TArray<FAssetCookManifestSourceRecord>& Records,
    int Depth)
{
    Out += "[";
    if (!Records.empty()) Out += "\n";
    for (Core::usize Index = 0; Index < Records.size(); ++Index)
    {
        const auto& Record = Records[Index];
        Indent(Out, Depth + 1); Out += "{\n";
        Key(Out, Depth + 2, "assetId"); Escape(Record.AssetId.ToString().View(), Out); Out += ",\n";
        Key(Out, Depth + 2, "version"); Escape(Record.Version.ToLowerHex().View(), Out); Out += ",\n";
        Key(Out, Depth + 2, "role"); Escape(Record.Role.View(), Out); Out += "\n";
        Indent(Out, Depth + 1); Out += Index + 1 == Records.size() ? "}\n" : "},\n";
    }
    if (!Records.empty()) Indent(Out, Depth);
    Out += "]";
}

void DependencyRecordsJson(
    std::string& Out,
    const Core::TArray<FAssetCookManifestDependencyRecord>& Records,
    int Depth)
{
    Out += "[";
    if (!Records.empty()) Out += "\n";
    for (Core::usize Index = 0; Index < Records.size(); ++Index)
    {
        const auto& Record = Records[Index];
        Indent(Out, Depth + 1); Out += "{\n";
        Key(Out, Depth + 2, "assetId"); Escape(Record.AssetId.ToString().View(), Out); Out += ",\n";
        Key(Out, Depth + 2, "role"); Escape(Record.Role.View(), Out);
        if (Record.RequiredVersion)
        {
            Out += ",\n";
            Key(Out, Depth + 2, "requiredVersion"); Escape(Record.RequiredVersion->ToLowerHex().View(), Out); Out += "\n";
        }
        else Out += "\n";
        Indent(Out, Depth + 1); Out += Index + 1 == Records.size() ? "}\n" : "},\n";
    }
    if (!Records.empty()) Indent(Out, Depth);
    Out += "]";
}

void RecordsJson(
    std::string& Out,
    const Core::TArray<FAssetCookManifestRecord>& Records,
    bool IncludeLocator)
{
    Out += "[";
    if (!Records.empty()) Out += "\n";
    for (Core::usize Index = 0; Index < Records.size(); ++Index)
    {
        const auto& Record = Records[Index];
        Indent(Out, 2); Out += "{\n";
        Key(Out, 3, "assetId"); Escape(Record.AssetId.ToString().View(), Out); Out += ",\n";
        Key(Out, 3, "assetType"); Escape(Record.AssetType.View(), Out); Out += ",\n";
        Key(Out, 3, "sourceVersion"); Escape(Record.SourceVersion.ToLowerHex().View(), Out); Out += ",\n";
        Key(Out, 3, "sourceManifest"); SourceRecordsJson(Out, Record.SourceManifest, 3); Out += ",\n";
        Key(Out, 3, "importer"); ParticipantJson(Out, Record.Importer, 3); Out += ",\n";
        Key(Out, 3, "cooker"); ParticipantJson(Out, Record.Cooker, 3); Out += ",\n";
        Key(Out, 3, "codec"); ParticipantJson(Out, Record.Codec, 3); Out += ",\n";
        Key(Out, 3, "derivedKey"); Escape(Record.DerivedKey.ToString().View(), Out); Out += ",\n";
        Key(Out, 3, "payloadSchemaVersion"); Out += std::to_string(Record.PayloadSchemaVersion); Out += ",\n";
        if (IncludeLocator)
        {
            Key(Out, 3, "payloadLocator"); Escape(Record.PayloadLocator.View(), Out); Out += ",\n";
        }
        Key(Out, 3, "payloadBytes"); Out += std::to_string(Record.PayloadBytes); Out += ",\n";
        Key(Out, 3, "envelopeDigest"); Escape(Record.EnvelopeDigest.ToLowerHex().View(), Out); Out += ",\n";
        Key(Out, 3, "dependencies"); DependencyRecordsJson(Out, Record.Dependencies, 3); Out += "\n";
        Indent(Out, 2); Out += Index + 1 == Records.size() ? "}\n" : "},\n";
    }
    if (!Records.empty()) Indent(Out, 1);
    Out += "]";
}

std::string IndentNested(std::string_view Text, int Depth)
{
    std::string Out;
    bool Start = true;
    for (const char Character : Text)
    {
        if (Start)
        {
            Indent(Out, Depth);
            Start = false;
        }
        Out.push_back(Character);
        if (Character == '\n') Start = true;
    }
    if (!Out.empty() && Out.back() == ' ') Out.pop_back();
    return Out;
}

std::string SemanticJson(const FAssetCookManifest& Manifest)
{
    std::string Out = "{\n";
    Key(Out, 1, "schema"); Escape(Manifest.Schema.View(), Out); Out += ",\n";
    Key(Out, 1, "schemaVersion"); Out += std::to_string(Manifest.SchemaVersion); Out += ",\n";
    Key(Out, 1, "effectiveProfileDigest"); Escape(Manifest.TargetProfile.EffectiveProfileDigest.ToLowerHex().View(), Out); Out += ",\n";
    Key(Out, 1, "selection"); SelectionJson(Out, Manifest.Selection, 1); Out += ",\n";
    Key(Out, 1, "snapshotDigest"); Escape(Manifest.SnapshotDigest.ToLowerHex().View(), Out); Out += ",\n";
    Key(Out, 1, "limitsDigest"); Escape(Manifest.LimitsDigest.ToLowerHex().View(), Out); Out += ",\n";
    Key(Out, 1, "records"); RecordsJson(Out, Manifest.Records, false); Out += ",\n";
    Key(Out, 1, "requiredExtensions"); StringArray(Out, Manifest.RequiredExtensions); Out += "\n}\n";
    return Out;
}

std::string ManifestJson(const FAssetCookManifest& Manifest)
{
    Core::FString ProfileText;
    (void)WriteAssetTargetProfile(Manifest.TargetProfile.Profile, ProfileText);
    std::string Out = "{\n";
    Key(Out, 1, "schema"); Escape(Manifest.Schema.View(), Out); Out += ",\n";
    Key(Out, 1, "schemaVersion"); Out += std::to_string(Manifest.SchemaVersion); Out += ",\n";
    Key(Out, 1, "generationId"); Escape(Manifest.GenerationId.ToLowerHex().View(), Out); Out += ",\n";
    Key(Out, 1, "targetProfile");
    std::string Nested = IndentNested(ProfileText.View(), 1);
    const std::size_t FirstBrace = Nested.find('{');
    Out += FirstBrace == std::string::npos ? "{}" : Nested.substr(FirstBrace);
    if (!Out.empty() && Out.back() == '\n') Out.pop_back();
    Out += ",\n";
    Key(Out, 1, "effectiveProfileDigest"); Escape(Manifest.TargetProfile.EffectiveProfileDigest.ToLowerHex().View(), Out); Out += ",\n";
    Key(Out, 1, "selection"); SelectionJson(Out, Manifest.Selection, 1); Out += ",\n";
    Key(Out, 1, "snapshotDigest"); Escape(Manifest.SnapshotDigest.ToLowerHex().View(), Out); Out += ",\n";
    Key(Out, 1, "limitsDigest"); Escape(Manifest.LimitsDigest.ToLowerHex().View(), Out); Out += ",\n";
    Key(Out, 1, "records"); RecordsJson(Out, Manifest.Records, true);
    if (!Manifest.RequiredExtensions.empty())
    {
        Out += ",\n";
        Key(Out, 1, "requiredExtensions"); StringArray(Out, Manifest.RequiredExtensions);
    }
    Out += "\n}\n";
    return Out;
}

bool ClosedObject(yyjson_val* Object, FAllowed Allowed)
{
    if (!yyjson_is_obj(Object)) return false;
    yyjson_obj_iter Iterator = yyjson_obj_iter_with(Object);
    yyjson_val* KeyValue = nullptr;
    while ((KeyValue = yyjson_obj_iter_next(&Iterator)) != nullptr)
    {
        const std::string_view Name(yyjson_get_str(KeyValue), yyjson_get_len(KeyValue));
        if (std::find(Allowed.begin(), Allowed.end(), Name) == Allowed.end())
            return false;
    }
    return true;
}

EAssetResult Preflight(yyjson_val* Root)
{
    struct Node { yyjson_val* Value; Core::usize Depth; };
    std::vector<Node> Stack{{Root, 1}};
    Core::usize Count = 0;
    while (!Stack.empty())
    {
        const Node Current = Stack.back();
        Stack.pop_back();
        if (++Count > 1000000 || Current.Depth > 32)
            return EAssetResult::DefinitionLimitExceeded;
        if (yyjson_is_obj(Current.Value))
        {
            std::set<std::string> Keys;
            yyjson_obj_iter Iterator = yyjson_obj_iter_with(Current.Value);
            yyjson_val* KeyValue = nullptr;
            while ((KeyValue = yyjson_obj_iter_next(&Iterator)) != nullptr)
            {
                if (!Keys.emplace(yyjson_get_str(KeyValue), yyjson_get_len(KeyValue)).second)
                    return EAssetResult::InvalidDefinition;
                Stack.push_back({yyjson_obj_iter_get_val(KeyValue), Current.Depth + 1});
            }
        }
        else if (yyjson_is_arr(Current.Value))
        {
            std::size_t Index = 0, Maximum = 0;
            yyjson_val* Value = nullptr;
            yyjson_arr_foreach(Current.Value, Index, Maximum, Value)
                Stack.push_back({Value, Current.Depth + 1});
        }
    }
    return EAssetResult::Success;
}

bool String(yyjson_val* Object, const char* Name, Core::FString& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_str(Value)) return false;
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
    const auto Result = std::from_chars(Begin, End, Parsed);
    if (Result.ec != std::errc{} || Result.ptr != End) return false;
    Out = Parsed;
    return true;
}

bool Digest(yyjson_val* Object, const char* Name, FAssetDigest& Out)
{
    Core::FString Text;
    return String(Object, Name, Text) &&
        FAssetDigest::ParseLowerHex(Text, Out) == EAssetResult::Success;
}

EAssetResult ParseAssetId(const Core::FString& Text, FAssetId& Out)
{
    Out = {};
    const auto View = Text.View();
    const std::size_t Colon = View.find(':');
    const std::size_t Hash = View.find('#', Colon == std::string_view::npos ? 0 : Colon + 1);
    if (Colon == std::string_view::npos || Colon == 0) return EAssetResult::InvalidIdentity;
    std::optional<Core::FString> Subresource;
    if (Hash != std::string_view::npos) Subresource = Core::FString(View.substr(Hash + 1));
    const EAssetResult Result = FAssetId::Create(
        Core::FString(View.substr(0, Colon)),
        Core::FString(View.substr(Colon + 1, Hash == std::string_view::npos ? std::string_view::npos : Hash - Colon - 1)),
        Subresource, Out);
    return Result == EAssetResult::Success && Out.ToString() == Text
        ? EAssetResult::Success : EAssetResult::InvalidIdentity;
}

bool AssetIdValue(yyjson_val* Value, FAssetId& Out)
{
    if (!yyjson_is_str(Value)) return false;
    return ParseAssetId(
        Core::FString(std::string(yyjson_get_str(Value), yyjson_get_len(Value))), Out) ==
        EAssetResult::Success;
}

bool Participant(yyjson_val* Object, FAssetCookManifestParticipant& Out)
{
    Core::FString Id, Version;
    return ClosedObject(Object, {"id", "version"}) &&
        String(Object, "id", Id) && String(Object, "version", Version) &&
        FAssetParticipantId::Create(Id, Out.Id) == EAssetResult::Success &&
        FAssetProducerVersion::Create(Version, Out.Version) == EAssetResult::Success;
}

bool ParseSelection(yyjson_val* Root, FAssetCookSelection& Out)
{
    yyjson_val* Object = yyjson_obj_get(Root, "selection");
    Core::FString Mode;
    if (!ClosedObject(Object, {"mode", "roots", "sourceScopes", "discoveryRulesVersion"}) ||
        !String(Object, "mode", Mode) ||
        (Mode != Core::FString("explicit-roots") && Mode != Core::FString("cook-all")) ||
        !Digest(Object, "discoveryRulesVersion", Out.DiscoveryRulesVersion))
        return false;
    Out.Mode = Mode == Core::FString("explicit-roots")
        ? EAssetCookSelectionMode::ExplicitRoots : EAssetCookSelectionMode::CookAll;
    yyjson_val* Roots = yyjson_obj_get(Object, "roots");
    yyjson_val* Scopes = yyjson_obj_get(Object, "sourceScopes");
    if (!yyjson_is_arr(Roots) || !yyjson_is_arr(Scopes)) return false;
    std::size_t Index = 0, Maximum = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Roots, Index, Maximum, Value)
    {
        FAssetId Id;
        if (!AssetIdValue(Value, Id)) return false;
        Out.Roots.push_back(std::move(Id));
    }
    yyjson_arr_foreach(Scopes, Index, Maximum, Value)
    {
        if (!yyjson_is_str(Value)) return false;
        Out.SourceScopes.emplace_back(std::string(yyjson_get_str(Value), yyjson_get_len(Value)));
    }
    return true;
}

bool ParseSourceRecords(yyjson_val* Array, Core::TArray<FAssetCookManifestSourceRecord>& Out)
{
    if (!yyjson_is_arr(Array)) return false;
    std::size_t Index = 0, Maximum = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Array, Index, Maximum, Value)
    {
        FAssetCookManifestSourceRecord Record;
        Core::FString Id;
        if (!ClosedObject(Value, {"assetId", "version", "role"}) ||
            !String(Value, "assetId", Id) ||
            ParseAssetId(Id, Record.AssetId) != EAssetResult::Success ||
            !Digest(Value, "version", Record.Version) ||
            !String(Value, "role", Record.Role)) return false;
        Out.push_back(std::move(Record));
    }
    return true;
}

bool ParseDependencies(yyjson_val* Array, Core::TArray<FAssetCookManifestDependencyRecord>& Out)
{
    if (!yyjson_is_arr(Array)) return false;
    std::size_t Index = 0, Maximum = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Array, Index, Maximum, Value)
    {
        FAssetCookManifestDependencyRecord Record;
        Core::FString Id;
        if (!ClosedObject(Value, {"assetId", "role", "requiredVersion"}) ||
            !String(Value, "assetId", Id) ||
            ParseAssetId(Id, Record.AssetId) != EAssetResult::Success ||
            !String(Value, "role", Record.Role)) return false;
        if (yyjson_obj_get(Value, "requiredVersion"))
        {
            FAssetDigest Required;
            if (!Digest(Value, "requiredVersion", Required)) return false;
            Record.RequiredVersion = Required;
        }
        Out.push_back(std::move(Record));
    }
    return true;
}

bool ParseRecords(yyjson_val* Root, Core::TArray<FAssetCookManifestRecord>& Out)
{
    yyjson_val* Array = yyjson_obj_get(Root, "records");
    if (!yyjson_is_arr(Array)) return false;
    std::size_t Index = 0, Maximum = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Array, Index, Maximum, Value)
    {
        FAssetCookManifestRecord Record;
        Core::FString Id, Derived;
        if (!ClosedObject(
                Value,
                {"assetId", "assetType", "sourceVersion", "sourceManifest",
                 "importer", "cooker", "codec", "derivedKey",
                 "payloadSchemaVersion", "payloadLocator", "payloadBytes",
                 "envelopeDigest", "dependencies"}) ||
            !String(Value, "assetId", Id) ||
            ParseAssetId(Id, Record.AssetId) != EAssetResult::Success ||
            !String(Value, "assetType", Record.AssetType) ||
            !Digest(Value, "sourceVersion", Record.SourceVersion) ||
            !ParseSourceRecords(yyjson_obj_get(Value, "sourceManifest"), Record.SourceManifest) ||
            !Participant(yyjson_obj_get(Value, "importer"), Record.Importer) ||
            !Participant(yyjson_obj_get(Value, "cooker"), Record.Cooker) ||
            !Participant(yyjson_obj_get(Value, "codec"), Record.Codec) ||
            !String(Value, "derivedKey", Derived) ||
            FAssetDerivedKey::ParseLowerHex(Derived, Record.DerivedKey) != EAssetResult::Success ||
            !Unsigned(Value, "payloadSchemaVersion", Record.PayloadSchemaVersion) ||
            !String(Value, "payloadLocator", Record.PayloadLocator) ||
            !Unsigned(Value, "payloadBytes", Record.PayloadBytes) ||
            !Digest(Value, "envelopeDigest", Record.EnvelopeDigest) ||
            !ParseDependencies(yyjson_obj_get(Value, "dependencies"), Record.Dependencies))
            return false;
        Out.push_back(std::move(Record));
    }
    return true;
}

bool ParseExtensions(yyjson_val* Root, Core::TArray<Core::FString>& Out)
{
    yyjson_val* Array = yyjson_obj_get(Root, "requiredExtensions");
    if (!Array) return true;
    if (!yyjson_is_arr(Array)) return false;
    std::size_t Index = 0, Maximum = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Array, Index, Maximum, Value)
    {
        if (!yyjson_is_str(Value)) return false;
        Out.emplace_back(std::string(yyjson_get_str(Value), yyjson_get_len(Value)));
    }
    return true;
}

} // namespace

EAssetResult ComputeAssetCookManifestGenerationId(
    const FAssetCookManifest& Manifest,
    FAssetDigest& OutGenerationId)
{
    OutGenerationId = {};
    const std::string Semantic = SemanticJson(Manifest);
    const auto* Begin = reinterpret_cast<const Core::uint8*>(Semantic.data());
    OutGenerationId = FAssetDigest::FromBytes(
        std::span<const Core::uint8>(Begin, Semantic.size()));
    return EAssetResult::Success;
}

EAssetResult WriteAssetCookManifest(
    FAssetCookManifest& InOutManifest,
    const FAssetCookManifestLimits& Limits,
    Core::FString& OutCanonical)
{
    OutCanonical.Clear();
    FAssetDigest Computed;
    (void)ComputeAssetCookManifestGenerationId(InOutManifest, Computed);
    if (InOutManifest.GenerationId.IsAvailable() &&
        InOutManifest.GenerationId != Computed)
        return EAssetResult::Conflict;
    InOutManifest.GenerationId = Computed;
    const EAssetResult Validation = InOutManifest.Validate(Limits);
    if (Validation != EAssetResult::Success) return Validation;
    const std::string Text = ManifestJson(InOutManifest);
    if (Text.size() > Limits.MaxManifestBytes)
        return EAssetResult::DefinitionLimitExceeded;
    OutCanonical = Core::FString(Text);
    return EAssetResult::Success;
}

EAssetResult ParseAssetCookManifest(
    std::span<const Core::uint8> Bytes,
    const FAssetCookManifestLimits& Limits,
    FAssetCookManifest& OutManifest)
{
    OutManifest = {};
    if (Limits.Validate() != EAssetResult::Success || Bytes.empty() ||
        Bytes.size() > Limits.MaxManifestBytes ||
        std::find(Bytes.begin(), Bytes.end(), Core::uint8{0}) != Bytes.end())
        return EAssetResult::DefinitionLimitExceeded;
    constexpr yyjson_read_flag Flags = YYJSON_READ_NUMBER_AS_RAW;
    const std::size_t Required = yyjson_read_max_memory_usage(Bytes.size(), Flags);
    std::vector<Core::uint8> Pool(Required);
    yyjson_alc Allocator{};
    if (Required == 0 || !yyjson_alc_pool_init(&Allocator, Pool.data(), Pool.size()))
        return EAssetResult::DefinitionLimitExceeded;
    yyjson_doc* Document = yyjson_read_opts(
        const_cast<char*>(reinterpret_cast<const char*>(Bytes.data())),
        Bytes.size(), Flags, &Allocator, nullptr);
    if (!Document) return EAssetResult::InvalidDefinition;
    yyjson_val* Root = yyjson_doc_get_root(Document);
    if (Preflight(Root) != EAssetResult::Success ||
        !ClosedObject(
            Root,
            {"schema", "schemaVersion", "generationId", "targetProfile",
             "effectiveProfileDigest", "selection", "snapshotDigest",
             "limitsDigest", "records", "requiredExtensions"}))
        return EAssetResult::InvalidDefinition;
    FAssetCookManifest Parsed;
    Core::FString GenerationText;
    if (!String(Root, "schema", Parsed.Schema) ||
        !Unsigned(Root, "schemaVersion", Parsed.SchemaVersion) ||
        !String(Root, "generationId", GenerationText) ||
        FAssetDigest::ParseLowerHex(GenerationText, Parsed.GenerationId) != EAssetResult::Success ||
        !ParseSelection(Root, Parsed.Selection) ||
        !Digest(Root, "snapshotDigest", Parsed.SnapshotDigest) ||
        !Digest(Root, "limitsDigest", Parsed.LimitsDigest) ||
        !ParseRecords(Root, Parsed.Records) ||
        !ParseExtensions(Root, Parsed.RequiredExtensions))
        return EAssetResult::InvalidDefinition;
    yyjson_val* ProfileValue = yyjson_obj_get(Root, "targetProfile");
    std::size_t ProfileBytes = 0;
    char* ProfileText = yyjson_val_write(ProfileValue, YYJSON_WRITE_NOFLAG, &ProfileBytes);
    if (!ProfileText) return EAssetResult::InvalidDefinition;
    const EAssetResult ProfileResult = ParseAssetTargetProfile(
        std::span<const Core::uint8>(
            reinterpret_cast<const Core::uint8*>(ProfileText), ProfileBytes),
        Parsed.TargetProfile);
    std::free(ProfileText);
    if (ProfileResult != EAssetResult::Success) return ProfileResult;
    FAssetDigest DeclaredEffective;
    if (!Digest(Root, "effectiveProfileDigest", DeclaredEffective) ||
        DeclaredEffective != Parsed.TargetProfile.EffectiveProfileDigest)
        return EAssetResult::Conflict;
    if (Parsed.SchemaVersion != FAssetCookManifest::CurrentSchemaVersion)
        return EAssetResult::UnsupportedSchema;
    FAssetDigest Computed;
    (void)ComputeAssetCookManifestGenerationId(Parsed, Computed);
    if (Computed != Parsed.GenerationId) return EAssetResult::Conflict;
    const EAssetResult Validation = Parsed.Validate(Limits);
    if (Validation != EAssetResult::Success) return Validation;
    const std::string Canonical = ManifestJson(Parsed);
    if (Canonical.size() != Bytes.size() ||
        !std::equal(Canonical.begin(), Canonical.end(), Bytes.begin()))
        return EAssetResult::InvalidDefinition;
    OutManifest = std::move(Parsed);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
