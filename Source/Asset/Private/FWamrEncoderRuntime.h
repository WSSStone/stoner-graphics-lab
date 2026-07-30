#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Core/TArray.h"

#include <span>

namespace Stoner::Asset::Private
{

struct FWamrEncoderResult
{
    EAssetResult Result = EAssetResult::CookFailure;
    Core::TArray<Core::uint8> Bytes;
    FAssetDiagnosticList Diagnostics;
};

class FWamrEncoderRuntime
{
public:
    static constexpr Core::uint32 ExpectedAbiVersion = 1;
    static constexpr const char* ExpectedModuleSha256 =
        "d394459dc8f85d2e133045c421b61ef6e080f5890c77f7f048a7258ee77e0b98";

    [[nodiscard]] static FWamrEncoderResult Execute(
        std::span<const Core::uint8> Request,
        Core::uint64 MaxOutputBytes);
};

} // namespace Stoner::Asset::Private
