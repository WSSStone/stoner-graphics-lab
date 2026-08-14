#pragma once

#include "AssetCooker/FAssetCookRequest.h"
#include "FAssetCookReportCodec.h"

#include <optional>
#include <span>

namespace Stoner::AssetCooker::Private
{

struct FAssetCookCliInvocation
{
    EAssetCookReportCommand Command = EAssetCookReportCommand::Cook;
    FAssetCookRequest CookRequest;
    Core::FString OutputRoot;
    Core::FString DerivedDataRoot;
    Core::FString TargetProfilePath;
    Core::FString ReportPath;
    std::optional<Asset::FAssetDigest> GenerationId;
    std::optional<Asset::FAssetDerivedKey> DerivedKey;
    Core::uint32 MaxErrors = 4096;
    bool bStrictFiles = false;
    bool bNormalizedReport = false;
};

struct FAssetCookCliResult
{
    EAssetCookResultCategory Category =
        EAssetCookResultCategory::InternalFailure;
    Core::FString StableReason;
    Core::FString CanonicalReport;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Category == EAssetCookResultCategory::Success;
    }
};

class FAssetCookCli
{
public:
    [[nodiscard]] static EAssetCookResultCategory Parse(
        std::span<const Core::FString> Arguments,
        FAssetCookCliInvocation& OutInvocation,
        Core::FString& OutReason);

    [[nodiscard]] static FAssetCookCliResult Execute(
        const FAssetCookCliInvocation& Invocation);
};

} // namespace Stoner::AssetCooker::Private
