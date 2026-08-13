#include "Asset/FStaticModelAsset.h"

#include "FStaticModelAssetValidator.h"

namespace Stoner::Asset
{

EAssetResult FStaticModelAsset::CreateValidated(
    FStaticModelAssetDesc Desc,
    Core::uint32 MaximumHierarchyDepth,
    FStaticModelAsset& OutAsset,
    FAssetDiagnosticList* Diagnostics)
{
    OutAsset = {};
    const EAssetResult Result = Private::ValidateStaticModelAsset(
        Desc, MaximumHierarchyDepth, Diagnostics);
    if (Result == EAssetResult::Success)
    {
        OutAsset.Desc_ = std::move(Desc);
    }
    return Result;
}

Core::FString FStaticModelAsset::GetAssetType() const
{
    return TAssetTypeTraits<FStaticModelAsset>::GetAssetType();
}

const FStaticModelAssetDesc& FStaticModelAsset::GetDesc() const noexcept
{
    return Desc_;
}

} // namespace Stoner::Asset
