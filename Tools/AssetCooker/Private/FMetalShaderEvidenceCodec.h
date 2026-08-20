#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FShaderNativeBindingEvidence.h"

#include <optional>

namespace Stoner::AssetCooker::Private
{

enum class EMetalShaderEvidenceKind : Core::uint8
{
    Derivation,
    NativeLibrary
};

struct FMetalNativeLibraryEvidence
{
    Core::FString Architecture;
    Core::FString Compiler;
    Core::FString XcodeBuild;
    Core::FString Sdk;
    Asset::FAssetDigest ArgumentDigest;
    Asset::FAssetDigest LibraryDigest;
    Core::uint64 SizeBytes = 0;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool operator==(
        const FMetalNativeLibraryEvidence&) const = default;
};

struct FMetalShaderEvidence
{
    Core::uint32 SchemaVersion = 1;
    EMetalShaderEvidenceKind Kind = EMetalShaderEvidenceKind::Derivation;
    Asset::FAssetId ShaderAssetId;
    Asset::FAssetDigest ShaderAssetVersion;
    std::optional<Asset::FAssetDigest> GlslDigest;
    Asset::FAssetDigest SpirvDigest;
    Asset::EShaderStage Stage = Asset::EShaderStage::Vertex;
    Core::FString EntryPoint;
    Asset::FAssetDigest InterfaceDigest;
    Core::FString SpirvCrossCommit = Core::FString(
        "a0fba56c34a6700f1724bf9b751da5b488a3775c");
    Asset::FAssetDigest SpirvCrossOptionsDigest;
    Asset::FShaderNativeBindingEvidence BindingEvidence;
    Core::FString TargetProfile;
    Core::FString DeploymentTarget = Core::FString("12.0");
    Core::FString MslVersion = Core::FString("2.4");
    Asset::FAssetDigest NormalizedMslDigest;
    std::optional<FMetalNativeLibraryEvidence> NativeLibrary;
    Asset::FAssetDigest EvidenceDigest;

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] Asset::EAssetResult FinalizeMetalShaderEvidence(
    FMetalShaderEvidence& Evidence) noexcept;

[[nodiscard]] Asset::EAssetResult WriteMetalShaderEvidence(
    const FMetalShaderEvidence& Evidence,
    Core::FString& OutCanonical) noexcept;

} // namespace Stoner::AssetCooker::Private
