#pragma once

#include "Asset/IAssetImporter.h"

namespace Stoner::Asset::Private
{

class FGLTFStaticModelImporter final : public IAssetImporter
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

} // namespace Stoner::Asset::Private
