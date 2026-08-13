#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FStaticModelImport.h"

#include <span>

namespace Stoner::Asset::Private
{

enum class EGLTFContainerType : Core::uint8
{
    JSON,
    GLB
};

struct FGLTFContainerPreflightResult
{
    EGLTFContainerType Type = EGLTFContainerType::JSON;
    Core::uint64 JsonOffset = 0;
    Core::uint64 JsonLength = 0;
    Core::uint64 BinaryOffset = 0;
    Core::uint64 BinaryLength = 0;
};

[[nodiscard]] EAssetResult PreflightGLTFContainer(
    std::span<const Core::uint8> Bytes,
    const FStaticModelImportLimits& Limits,
    FGLTFContainerPreflightResult& OutResult,
    FAssetDiagnosticList* OutDiagnostics = nullptr);

} // namespace Stoner::Asset::Private
