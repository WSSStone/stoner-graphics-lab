#include "FCurrentGenerationPointerCodec.h"

#include "../../../ThirdParty/yyjson/yyjson.h"

#include <algorithm>
#include <charconv>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Stoner::Asset::Private
{
namespace
{

bool Closed(yyjson_val* Object)
{
    if (!yyjson_is_obj(Object) || yyjson_obj_size(Object) != 5) return false;
    const std::set<std::string_view> Allowed{
        "schema", "schemaVersion", "generationId", "manifestLocator", "manifestDigest"};
    std::set<std::string_view> Seen;
    yyjson_obj_iter Iterator = yyjson_obj_iter_with(Object);
    yyjson_val* Key = nullptr;
    while ((Key = yyjson_obj_iter_next(&Iterator)) != nullptr)
    {
        const std::string_view Name(yyjson_get_str(Key), yyjson_get_len(Key));
        if (!Allowed.contains(Name) || !Seen.insert(Name).second) return false;
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

} // namespace

EAssetResult WriteCurrentGenerationPointer(
    const FCurrentGenerationPointer& Pointer,
    Core::FString& OutCanonical)
{
    OutCanonical = {};
    if (Pointer.Validate() != EAssetResult::Success) return EAssetResult::InvalidInput;
    const std::string Text =
        "{\"schema\":\"stoner.asset-current-generation\",\"schemaVersion\":1,"
        "\"generationId\":\"" + Pointer.GenerationId.ToLowerHex().ToStdString() +
        "\",\"manifestLocator\":\"" + Pointer.ManifestLocator.ToStdString() +
        "\",\"manifestDigest\":\"" + Pointer.ManifestDigest.ToLowerHex().ToStdString() + "\"}\n";
    OutCanonical = Core::FString(Text);
    return EAssetResult::Success;
}

EAssetResult ParseCurrentGenerationPointer(
    std::span<const Core::uint8> Bytes,
    FCurrentGenerationPointer& OutPointer)
{
    OutPointer = {};
    if (Bytes.empty() || Bytes.size() > 1024ULL * 1024ULL ||
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
    FCurrentGenerationPointer Parsed;
    Core::FString Generation, ManifestDigest;
    yyjson_val* VersionValue = yyjson_obj_get(Root, "schemaVersion");
    bool Valid = Closed(Root) && Text(Root, "schema", Parsed.Schema) &&
        yyjson_is_raw(VersionValue);
    if (Valid)
    {
        const char* Begin = yyjson_get_raw(VersionValue);
        const char* End = Begin + yyjson_get_len(VersionValue);
        const auto [Position, Error] = std::from_chars(
            Begin, End, Parsed.SchemaVersion);
        Valid = Error == std::errc{} && Position == End &&
            Text(Root, "generationId", Generation) &&
            Text(Root, "manifestLocator", Parsed.ManifestLocator) &&
            Text(Root, "manifestDigest", ManifestDigest) &&
            FAssetDigest::ParseLowerHex(Generation, Parsed.GenerationId) == EAssetResult::Success &&
            FAssetDigest::ParseLowerHex(ManifestDigest, Parsed.ManifestDigest) == EAssetResult::Success &&
            Parsed.Validate() == EAssetResult::Success;
    }
    Core::FString Canonical;
    if (Valid) Valid = WriteCurrentGenerationPointer(Parsed, Canonical) == EAssetResult::Success &&
        Canonical.Len() == Bytes.size() &&
        std::equal(Bytes.begin(), Bytes.end(), Canonical.View().begin());
    yyjson_doc_free(Document);
    if (!Valid) return EAssetResult::InvalidDefinition;
    OutPointer = std::move(Parsed);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
