#pragma once

#include "Asset/FAssetSource.h"
#include "Core/TSharedPtr.h"

namespace Stoner::Asset
{

class FAssetImportParameters
{
public:
    virtual ~FAssetImportParameters() = default;
};

struct FAssetImportRequest
{
    FAssetSourceDescriptor Descriptor;
    FAssetSourceLease Source;
    Core::TSharedPtr<const FAssetImportParameters> Parameters;
};

} // namespace Stoner::Asset
