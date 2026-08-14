#pragma once

#include "Asset/AssetMinimal.h"

#include <algorithm>
#include <span>
#include <string>

namespace Stoner::Tests::AssetCooker
{

class FSyntheticPayload final : public Asset::FAssetPayload
{
public:
    explicit FSyntheticPayload(Core::FString Type)
        : Type_(std::move(Type))
    {
    }

    [[nodiscard]] Core::FString GetAssetType() const override { return Type_; }

private:
    Core::FString Type_;
};

class FMemorySource final : public Asset::IAssetSource
{
public:
    explicit FMemorySource(Core::TArray<Core::uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    Asset::EAssetResult Read(
        Core::uint64 Offset,
        Core::usize MaximumBytes,
        Core::TArray<Core::uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size()) return Asset::EAssetResult::MalformedSource;
        const Core::usize Count = std::min(
            MaximumBytes, Bytes_.size() - static_cast<Core::usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return Asset::EAssetResult::Success;
    }

private:
    Core::TArray<Core::uint8> Bytes_;
};

inline Asset::FAssetDigest Digest(std::string_view Text)
{
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
}

inline Asset::FAssetId Id(const char* Type, const char* Path)
{
    Asset::FAssetId Value;
    (void)Asset::FAssetId::Create(
        Core::FString(Type), Core::FString(Path), {}, Value);
    return Value;
}

inline Asset::FAssetSourceLocator Locator(const char* Path)
{
    Asset::FAssetSourceLocator Value;
    (void)Asset::FAssetSourceLocator::Create(
        Core::FString("fixture"), Core::FString(Path), Value);
    return Value;
}

inline Asset::FAssetImportOutput Output(
    const Asset::FAssetId& AssetId,
    Core::TArray<Asset::FAssetDependency> Dependencies = {})
{
    Asset::FAssetMetadata Metadata;
    Metadata.Id = AssetId;
    Metadata.Source = Locator(AssetId.ToString().CStr());
    (void)Asset::FAssetParticipantId::Create(
        Core::FString("importer.synthetic"), Metadata.Producer);
    (void)Asset::FAssetProducerVersion::Create(
        Core::FString("test-v1"), Metadata.ProducerVersion);
    Metadata.Version.SourceDigest = Digest(AssetId.ToString().View());
    Metadata.Version.ContentDigest = Metadata.Version.SourceDigest;
    Metadata.Dependencies = std::move(Dependencies);
    return {
        std::move(Metadata),
        Core::MakeShared<FSyntheticPayload>(AssetId.GetAssetType())};
}

inline Asset::FAssetDependency Required(const Asset::FAssetId& Target)
{
    return {
        Target,
        Asset::EAssetDependencyRole::Build,
        Asset::EAssetDependencyStrength::Required,
        Asset::EAssetDependencyResolution::Resolved};
}

inline Asset::FAssetSourceDescriptor Descriptor(
    const char* Path,
    Core::usize Size)
{
    Asset::FAssetSourceDescriptor Value;
    Value.Location = Locator(Path);
    Value.Size = Size;
    return Value;
}

} // namespace Stoner::Tests::AssetCooker
