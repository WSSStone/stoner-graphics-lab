#pragma once

#include "Asset/FAssetDispatch.h"
#include "Asset/FAssetRegistry.h"
#include "Asset/FImageAsset.h"
#include "Asset/FImageInspection.h"
#include "Asset/FTextureAsset.h"

namespace Stoner::Asset
{

struct FImageImportParameters final : FAssetImportParameters
{
    FAssetId ImageId;
    FAssetId TextureId;
    FImageImportSettings Settings;
};

struct FImageImportResult
{
    EAssetResult Result = EAssetResult::ProcessingFailure;
    Core::TSharedPtr<const FImageAsset> Image;
    Core::TSharedPtr<const FTextureAsset> Texture;
    Core::uint64 RegistryRevision = 0;
    FAssetDiagnosticList Diagnostics;
};

class FImageAssetImporter final : public IAssetImporter
{
public:
    [[nodiscard]] FAssetExtensionCapability GetCapability() const override;
    [[nodiscard]] FAssetProbeResult Probe(
        const FAssetSourceDescriptor& Descriptor,
        std::span<const Core::uint8> Prefix) override;
    [[nodiscard]] EAssetResult Import(
        const FAssetSourceDescriptor& Descriptor,
        const FAssetSourceLease& Source,
        Core::TArray<FAssetImportOutput>& OutOutputs) override;
    [[nodiscard]] EAssetResult Import(
        const FAssetImportRequest& Request,
        Core::TArray<FAssetImportOutput>& OutOutputs) override;
    [[nodiscard]] EAssetResult Import(
        const FAssetImportRequest& Request,
        Core::TArray<FAssetImportOutput>& OutOutputs,
        FAssetDiagnosticList* Diagnostics) override;
};

class FAssetImportService
{
public:
    [[nodiscard]] static FImageImportResult ImportAndRegister(
        const FAssetExtensionRegistry& Extensions,
        FAssetRegistry& Registry,
        const FAssetImportRequest& Request);
};

[[nodiscard]] EAssetResult RegisterImageAssetImporter(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken);

} // namespace Stoner::Asset
