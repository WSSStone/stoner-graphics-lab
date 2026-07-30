#include "Asset/AssetMinimal.h"

namespace Stoner::Asset
{

EAssetResult RegisterKTX2TextureCooker(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken)
{
    return Registry.Register(
        Core::MakeShared<FKTX2TextureCooker>(),
        OutToken);
}

EAssetResult RegisterKTX2TextureLoader(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken)
{
    return Registry.Register(
        Core::MakeShared<FKTX2TextureLoader>(),
        OutToken);
}

} // namespace Stoner::Asset
