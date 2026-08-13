#include "Asset/AssetMinimal.h"

#include "FGLTFStaticModelImporter.h"

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

EAssetResult RegisterStaticModelImporter(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken)
{
    return Registry.Register(
        Core::MakeShared<Private::FGLTFStaticModelImporter>(),
        OutToken);
}

} // namespace Stoner::Asset
