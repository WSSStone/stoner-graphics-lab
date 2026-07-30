#pragma once

#include "Asset/FKTX2TextureCodec.h"
#include "Asset/IAssetLoader.h"

namespace Stoner::Asset
{

struct FKTX2LoadParameters final : FAssetLoadParameters
{
    FAssetId ExpectedId;
    FTextureCookLimits Limits;
};

class FKTX2TextureLoader final : public IAssetLoader
{
public:
    [[nodiscard]] FAssetExtensionCapability
        GetCapability() const override;
    [[nodiscard]] FAssetLoadResult Load(
        const FAssetLoadRequest& Request) override;
};

[[nodiscard]] EAssetResult RegisterKTX2TextureLoader(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken);

} // namespace Stoner::Asset
