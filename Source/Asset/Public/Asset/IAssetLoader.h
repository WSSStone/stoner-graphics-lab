#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSource.h"

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
};

struct FAssetLoadResult
{
    EAssetResult Result = EAssetResult::Unsupported;
    Core::TSharedPtr<const FAssetPayload> Payload;
    FAssetDiagnosticList Diagnostics;
};

class IAssetLoader : public IAssetExtension
{
public:
    [[nodiscard]] virtual FAssetLoadResult Load(const FAssetLoadRequest& Request) = 0;
};

} // namespace Stoner::Asset
