#include "Asset/FShaderPayloadAsset.h"
#include "Asset/FShaderSourceAsset.h"

#include "Core/FUnicode.h"

#include <algorithm>

namespace Stoner::Asset
{
namespace
{

bool IsCanonicalPermutation(const FShaderPermutationKey& Key)
{
    return std::is_sorted(Key.Flags.begin(), Key.Flags.end()) &&
        std::adjacent_find(Key.Flags.begin(), Key.Flags.end()) == Key.Flags.end();
}

} // namespace

EAssetResult FShaderSourceAsset::Create(
    FAssetId Id,
    FAssetVersion Version,
    EShaderSourceLanguage Language,
    Core::TArray<Core::uint8> Bytes,
    FShaderSourceAsset& OutAsset)
{
    OutAsset = {};
    Core::FString Text(std::string(
        reinterpret_cast<const char*>(Bytes.data()), Bytes.size()));
    Core::FString Normalized;
    if (!Id.IsValid() ||
        Id.GetAssetType() != TAssetTypeTraits<FShaderSourceAsset>::GetAssetType() ||
        Version.Validate() != EAssetResult::Success ||
        !Version.SourceDigest.IsAvailable() ||
        Language != EShaderSourceLanguage::GLSL ||
        Bytes.empty() ||
        Core::FUnicode::NormalizeNFC(Text, Normalized) !=
            Core::EUnicodeResult::Success)
    {
        return EAssetResult::InvalidInput;
    }
    OutAsset.Id_ = std::move(Id);
    OutAsset.Version_ = std::move(Version);
    OutAsset.Language_ = Language;
    OutAsset.Bytes_ = std::move(Bytes);
    return EAssetResult::Success;
}

Core::FString FShaderSourceAsset::GetAssetType() const
{
    return TAssetTypeTraits<FShaderSourceAsset>::GetAssetType();
}
const FAssetId& FShaderSourceAsset::GetId() const noexcept { return Id_; }
const FAssetVersion& FShaderSourceAsset::GetVersion() const noexcept { return Version_; }
EShaderSourceLanguage FShaderSourceAsset::GetLanguage() const noexcept { return Language_; }
const Core::TArray<Core::uint8>& FShaderSourceAsset::GetBytes() const noexcept { return Bytes_; }

EAssetResult FShaderPayloadAsset::Create(
    FAssetId Id,
    FAssetVersion Version,
    EShaderBackendFamily Backend,
    Core::FString Profile,
    EShaderPayloadFormat Format,
    EShaderStage Stage,
    Core::FString EntryPoint,
    FShaderPermutationKey Permutation,
    Core::TArray<Core::uint8> Bytes,
    FShaderPayloadAsset& OutAsset)
{
    OutAsset = {};
    if (!Id.IsValid() ||
        Id.GetAssetType() != TAssetTypeTraits<FShaderPayloadAsset>::GetAssetType() ||
        Version.Validate() != EAssetResult::Success ||
        !Version.ContentDigest.IsAvailable() ||
        Profile.IsEmpty() ||
        EntryPoint.IsEmpty() ||
        Bytes.empty() ||
        !IsCanonicalPermutation(Permutation))
    {
        return EAssetResult::InvalidInput;
    }
    OutAsset.Id_ = std::move(Id);
    OutAsset.Version_ = std::move(Version);
    OutAsset.Backend_ = Backend;
    OutAsset.Profile_ = std::move(Profile);
    OutAsset.Format_ = Format;
    OutAsset.Stage_ = Stage;
    OutAsset.EntryPoint_ = std::move(EntryPoint);
    OutAsset.Permutation_ = std::move(Permutation);
    OutAsset.Bytes_ = std::move(Bytes);
    return EAssetResult::Success;
}

Core::FString FShaderPayloadAsset::GetAssetType() const
{
    return TAssetTypeTraits<FShaderPayloadAsset>::GetAssetType();
}
const FAssetId& FShaderPayloadAsset::GetId() const noexcept { return Id_; }
const FAssetVersion& FShaderPayloadAsset::GetVersion() const noexcept { return Version_; }
EShaderBackendFamily FShaderPayloadAsset::GetBackend() const noexcept { return Backend_; }
const Core::FString& FShaderPayloadAsset::GetProfile() const noexcept { return Profile_; }
EShaderPayloadFormat FShaderPayloadAsset::GetFormat() const noexcept { return Format_; }
EShaderStage FShaderPayloadAsset::GetStage() const noexcept { return Stage_; }
const Core::FString& FShaderPayloadAsset::GetEntryPoint() const noexcept { return EntryPoint_; }
const FShaderPermutationKey& FShaderPayloadAsset::GetPermutation() const noexcept { return Permutation_; }
const Core::TArray<Core::uint8>& FShaderPayloadAsset::GetBytes() const noexcept { return Bytes_; }

} // namespace Stoner::Asset
