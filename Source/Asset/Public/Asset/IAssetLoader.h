#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSource.h"

namespace Stoner::Asset
{

struct FAssetLoadRequest
{
    FAssetMetadata Metadata;
    FAssetSourceLease Source;
};

struct FAssetLoadResult
{
    EAssetResult Result = EAssetResult::Unsupported;
    Core::TSharedPtr<const FAssetPayload> Payload;
};

class IAssetLoader : public IAssetExtension
{
public:
    [[nodiscard]] virtual FAssetLoadResult Load(const FAssetLoadRequest& Request) = 0;
};

} // namespace Stoner::Asset
