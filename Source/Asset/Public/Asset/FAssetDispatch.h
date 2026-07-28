#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/IAssetCooker.h"
#include "Asset/IAssetImporter.h"
#include "Asset/IAssetLoader.h"
#include "Asset/IAssetResolver.h"

namespace Stoner::Asset
{

class FAssetDispatch
{
public:
    [[nodiscard]] static FAssetResolveResult Resolve(
        const FAssetExtensionRegistry& Registry,
        const FAssetResolveRequest& Request,
        FAssetDiagnosticList* Diagnostics = nullptr);

    [[nodiscard]] static EAssetResult Import(
        const FAssetExtensionRegistry& Registry,
        const FAssetSourceDescriptor& Descriptor,
        const FAssetSourceLease& Source,
        Core::TArray<FAssetImportOutput>& OutOutputs,
        FAssetDiagnosticList* Diagnostics = nullptr);

    [[nodiscard]] static FAssetLoadResult Load(
        const FAssetExtensionRegistry& Registry,
        const FAssetParticipantId& Participant,
        const FAssetLoadRequest& Request);

    [[nodiscard]] static FAssetCookResult Cook(
        const FAssetExtensionRegistry& Registry,
        const FAssetParticipantId& Participant,
        const FAssetCookRequest& Request);
};

} // namespace Stoner::Asset
