#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetImportRequest.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"

#include <span>

namespace Stoner::Asset
{

struct FAssetProbeResult
{
    EAssetResult Result = EAssetResult::Success;
    int Confidence = 0;
    Core::FString Reason;
};

struct FAssetImportOutput
{
    FAssetMetadata Metadata;
    Core::TSharedPtr<const FAssetPayload> Payload;
};

class IAssetImporter : public IAssetExtension
{
public:
    [[nodiscard]] virtual FAssetProbeResult Probe(
        const FAssetSourceDescriptor& Descriptor,
        std::span<const Core::uint8> Prefix) = 0;
    [[nodiscard]] virtual EAssetResult Import(
        const FAssetSourceDescriptor& Descriptor,
        const FAssetSourceLease& Source,
        Core::TArray<FAssetImportOutput>& OutOutputs) = 0;

    [[nodiscard]] virtual EAssetResult Import(
        const FAssetImportRequest& Request,
        Core::TArray<FAssetImportOutput>& OutOutputs)
    {
        return Import(Request.Descriptor, Request.Source, OutOutputs);
    }

    [[nodiscard]] virtual EAssetResult Import(
        const FAssetImportRequest& Request,
        Core::TArray<FAssetImportOutput>& OutOutputs,
        FAssetDiagnosticList* Diagnostics)
    {
        (void)Diagnostics;
        return Import(Request, OutOutputs);
    }
};

} // namespace Stoner::Asset
