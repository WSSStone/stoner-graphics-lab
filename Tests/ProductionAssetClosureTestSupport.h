#pragma once

#include "Asset/AssetMinimal.h"

#include <filesystem>
#include <variant>

struct FProductionAssetExtensionSet
{
    Stoner::Core::TSharedPtr<Stoner::Asset::FAssetExtensionRegistry> Registry;
    Stoner::Asset::FAssetRegistrationToken Resolver;
    Stoner::Asset::FAssetRegistrationToken ImageImporter;
    Stoner::Asset::FAssetRegistrationToken MaterialImporter;
    Stoner::Asset::FAssetRegistrationToken StaticModelImporter;
    Stoner::Asset::FAssetRegistrationToken KTX2Loader;
    Stoner::Asset::FAssetCookedExtensionRegistrations Cooked;

    [[nodiscard]] bool IsComplete() const noexcept;
};

using FProductionAssetHandle = std::variant<
    Stoner::Asset::TAssetHandle<Stoner::Asset::FImageAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FTextureAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FKTX2TextureArtifact>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FShaderSourceAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FShaderPayloadAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FShaderAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FMaterialAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FMaterialInstanceAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FStaticMeshAsset>,
    Stoner::Asset::TAssetHandle<Stoner::Asset::FStaticModelAsset>>;

struct FProductionAssetClosureEntry
{
    Stoner::Asset::FAssetId AssetId;
    Stoner::Core::FString AssetType;
    FProductionAssetHandle Handle;

    [[nodiscard]] const Stoner::Asset::FAssetPayload* GetPayload() const;
};

struct FProductionAssetClosure
{
    Stoner::Asset::FAssetDigest GenerationIdentity;
    Stoner::Core::TArray<FProductionAssetClosureEntry> Entries;

    [[nodiscard]] const FProductionAssetClosureEntry* Find(
        const Stoner::Asset::FAssetId& AssetId) const;
};

[[nodiscard]] bool CreateProductionAssetExtensionSet(
    const std::filesystem::path& PackageRoot,
    const std::filesystem::path& ShaderRoot,
    FProductionAssetExtensionSet& Out);

[[nodiscard]] bool LoadProductionAssetClosure(
    Stoner::Asset::FAssetManager& Manager,
    const Stoner::Asset::FAssetCookManifest& Manifest,
    bool bStrictCooked,
    FProductionAssetClosure& Out,
    Stoner::Core::FString& OutFailure,
    Stoner::Core::uint64 RequestTimeoutMilliseconds = 30000);
