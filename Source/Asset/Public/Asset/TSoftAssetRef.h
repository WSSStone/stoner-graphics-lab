#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"

#include <optional>

namespace Stoner::Asset
{

template <CTypedAssetPayload T>
class TSoftAssetRef
{
public:
    TSoftAssetRef() = default;

    [[nodiscard]] static EAssetResult Create(
        const FAssetId& Id,
        TSoftAssetRef& OutReference)
    {
        OutReference = {};
        if (!Id.IsValid())
        {
            return EAssetResult::InvalidIdentity;
        }
        if (Id.GetAssetType() != TAssetTypeTraits<T>::GetAssetType())
        {
            return EAssetResult::TypeMismatch;
        }
        OutReference.Id_ = Id;
        return EAssetResult::Success;
    }

    [[nodiscard]] bool IsEmpty() const noexcept { return !Id_.has_value(); }
    [[nodiscard]] const std::optional<FAssetId>& GetId() const noexcept { return Id_; }

private:
    std::optional<FAssetId> Id_;
};

} // namespace Stoner::Asset
