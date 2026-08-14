#pragma once

#include "Asset/FAssetSource.h"
#include "Asset/IAssetResolver.h"

#include <string_view>

namespace Stoner::Asset::Private
{

struct FGLTFResolvedDependency
{
    FAssetSourceDescriptor Descriptor;
    FAssetSourceLease Source;
    Core::TArray<Core::uint8> Bytes;
};

[[nodiscard]] EAssetResult DecodeGLTFDataUri(
    std::string_view Uri,
    Core::uint64 MaximumBytes,
    Core::TArray<Core::uint8>& OutBytes);

[[nodiscard]] EAssetResult ResolveGLTFDependency(
    const FAssetSourceLocator& MainSource,
    std::string_view RelativeUri,
    const Core::TSharedPtr<IAssetResolver>& Resolver,
    Core::uint64 MaximumBytes,
    FGLTFResolvedDependency& OutDependency);

} // namespace Stoner::Asset::Private
