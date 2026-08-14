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

EAssetResult ImportStaticModel(
    const FStaticModelImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    Private::FGLTFStaticModelImporter Importer;
    return Importer.Import(Request, OutOutputs, Diagnostics);
}

} // namespace Stoner::Asset
