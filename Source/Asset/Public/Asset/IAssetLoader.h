#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSource.h"
#include "Asset/FAssetTargetProfile.h"
#include "Asset/FAssetRuntimeExecutionContext.h"

#include <utility>

namespace Stoner::Asset
{

class FAssetLoadParameters
{
public:
    virtual ~FAssetLoadParameters() = default;
};

struct FAssetLoadRequest
{
    FAssetMetadata Metadata;
    FAssetSourceLease Source;
    Core::TSharedPtr<const FAssetLoadParameters> Parameters;
    Core::TSharedPtr<const FAssetTargetProfileEvidence> TargetProfileEvidence;
    Core::TSharedPtr<const FAssetRuntimeExecutionContext> RuntimeContext;
};

struct FAssetLoadResult
{
    FAssetLoadResult() = default;
    FAssetLoadResult(
        EAssetResult InResult,
        Core::TSharedPtr<const FAssetPayload> InPayload,
        FAssetDiagnosticList InDiagnostics)
        : Result(InResult),
          Payload(std::move(InPayload)),
          Diagnostics(std::move(InDiagnostics))
    {
    }

    EAssetResult Result = EAssetResult::Unsupported;
    Core::TSharedPtr<const FAssetPayload> Payload;
    FAssetDiagnosticList Diagnostics;
    Core::TSharedPtr<const FAssetTargetProfileEvidence> TargetProfileEvidence;
};

class IAssetLoader : public IAssetExtension
{
public:
    [[nodiscard]] virtual FAssetLoadResult Load(const FAssetLoadRequest& Request) = 0;
};

} // namespace Stoner::Asset
