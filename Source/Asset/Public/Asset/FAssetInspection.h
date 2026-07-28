#pragma once

#include "Asset/FAssetRegistry.h"

namespace Stoner::Asset
{

struct FAssetExtensionCapability;
enum class EAssetExtensionKind : Core::uint8;

class FAssetInspection
{
public:
    [[nodiscard]] static Core::FString Format(const FAssetId& Id);
    [[nodiscard]] static Core::FString Format(const FAssetDigest& Digest);
    [[nodiscard]] static Core::FString Format(const FAssetVersion& Version);
    [[nodiscard]] static Core::FString Format(const FAssetDependency& Dependency);
    [[nodiscard]] static Core::FString Format(const FAssetMetadata& Metadata);
    [[nodiscard]] static Core::FString Format(const FAssetRegistrySnapshot& Snapshot);
    [[nodiscard]] static Core::FString FormatCapabilities(
        const Core::TArray<FAssetExtensionCapability>& Capabilities);
    [[nodiscard]] static Core::FString FormatAmbiguity(
        EAssetExtensionKind Kind,
        const Core::TArray<FAssetExtensionCapability>& Candidates);
};

} // namespace Stoner::Asset
