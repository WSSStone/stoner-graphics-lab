#pragma once

#include "Core/FString.h"

#include <concepts>

namespace Stoner::Asset
{

class FAssetPayload
{
public:
    virtual ~FAssetPayload() = default;
    [[nodiscard]] virtual Core::FString GetAssetType() const = 0;
};

template <typename T>
struct TAssetTypeTraits;

template <typename T>
concept CTypedAssetPayload = requires
{
    { TAssetTypeTraits<T>::GetAssetType() } -> std::same_as<Core::FString>;
};

} // namespace Stoner::Asset
