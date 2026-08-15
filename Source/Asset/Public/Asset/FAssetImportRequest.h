#pragma once

#include "Asset/FAssetSource.h"
#include "Asset/FAssetRuntimeExecutionContext.h"
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
    Core::TSharedPtr<const FAssetRuntimeExecutionContext> RuntimeContext;
};

} // namespace Stoner::Asset
