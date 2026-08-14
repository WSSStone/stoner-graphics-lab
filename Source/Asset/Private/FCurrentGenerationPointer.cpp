#include "Asset/FCurrentGenerationPointer.h"

namespace Stoner::Asset
{

EAssetResult FCurrentGenerationPointer::Validate() const noexcept
{
    if (Schema != Core::FString("stoner.asset-current-generation") ||
        SchemaVersion != CurrentSchemaVersion || !GenerationId.IsAvailable() ||
        !ManifestDigest.IsAvailable()) return EAssetResult::InvalidInput;
    const Core::FString Expected(
        "Generations/" + GenerationId.ToLowerHex().ToStdString() +
        "/Manifest.json");
    return ManifestLocator == Expected
        ? EAssetResult::Success : EAssetResult::InvalidInput;
}

} // namespace Stoner::Asset
