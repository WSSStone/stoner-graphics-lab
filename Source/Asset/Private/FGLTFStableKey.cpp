#include "FGLTFStableKey.h"

#include "Asset/FAssetId.h"

#include "../../../ThirdParty/yyjson/yyjson.h"

#include <optional>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

EAssetResult ValidateKey(const std::string& Key, Core::FString& OutKey)
{
    FAssetId TestId;
    const EAssetResult Result = FAssetId::Create(
        Core::FString("StaticModelKey"),
        Core::FString("Validation/StableKey"),
        std::optional<Core::FString>(Core::FString(Key)),
        TestId);
    if (Result != EAssetResult::Success || !TestId.GetSubresource())
    {
        return Result == EAssetResult::Success
            ? EAssetResult::InvalidIdentity : Result;
    }
    OutKey = *TestId.GetSubresource();
    return EAssetResult::Success;
}

} // namespace

EAssetResult MakeGLTFStableKey(
    const char* ExtrasJson,
    const Core::FString& FallbackKey,
    Core::FString& OutKey,
    bool& OutExplicit)
{
    OutKey.Clear();
    OutExplicit = false;
    if (ExtrasJson == nullptr)
    {
        return ValidateKey(FallbackKey.ToStdString(), OutKey);
    }
    yyjson_read_err Error{};
    yyjson_doc* Document = yyjson_read_opts(
        const_cast<char*>(ExtrasJson),
        std::char_traits<char>::length(ExtrasJson),
        YYJSON_READ_NOFLAG,
        nullptr,
        &Error);
    if (Document == nullptr)
    {
        return EAssetResult::MalformedSource;
    }
    yyjson_val* Root = yyjson_doc_get_root(Document);
    yyjson_val* Value = yyjson_is_obj(Root)
        ? yyjson_obj_get(Root, "stonerAssetId") : nullptr;
    EAssetResult Result = EAssetResult::Success;
    if (Value == nullptr)
    {
        Result = ValidateKey(FallbackKey.ToStdString(), OutKey);
    }
    else if (!yyjson_is_str(Value) || yyjson_get_len(Value) == 0)
    {
        Result = EAssetResult::InvalidIdentity;
    }
    else
    {
        OutExplicit = true;
        Result = ValidateKey(
            "key." + std::string(yyjson_get_str(Value), yyjson_get_len(Value)),
            OutKey);
    }
    yyjson_doc_free(Document);
    return Result;
}

} // namespace Stoner::Asset::Private
