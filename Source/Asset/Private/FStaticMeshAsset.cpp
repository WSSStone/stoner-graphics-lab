#include "Asset/FStaticMeshAsset.h"

#include "FStaticMeshAssetValidator.h"

namespace Stoner::Asset
{

EAssetResult FStaticMeshAsset::CreateValidated(
    FStaticMeshAssetDesc Desc,
    FStaticMeshAsset& OutAsset,
    FAssetDiagnosticList* Diagnostics)
{
    OutAsset = {};
    const EAssetResult Result =
        Private::ValidateStaticMeshAsset(Desc, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    OutAsset.Desc_ = std::move(Desc);
    return EAssetResult::Success;
}

Core::FString FStaticMeshAsset::GetAssetType() const
{
    return TAssetTypeTraits<FStaticMeshAsset>::GetAssetType();
}

const FStaticMeshAssetDesc& FStaticMeshAsset::GetDesc() const noexcept
{
    return Desc_;
}

} // namespace Stoner::Asset
