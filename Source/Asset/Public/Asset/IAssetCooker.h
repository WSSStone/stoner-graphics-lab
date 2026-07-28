#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"

namespace Stoner::Asset
{

struct FAssetCookRequest
{
    FAssetMetadata Metadata;
    Core::TSharedPtr<const FAssetPayload> Payload;
    Core::FString TargetProfile;
};

struct FAssetCookResult
{
    EAssetResult Result = EAssetResult::Unsupported;
    Core::FString TargetProfile;
    Core::TArray<Core::uint8> Artifact;
    FAssetDigest CookDigest;
};

class IAssetCooker : public IAssetExtension
{
public:
    [[nodiscard]] virtual FAssetCookResult Cook(const FAssetCookRequest& Request) = 0;
};

} // namespace Stoner::Asset
