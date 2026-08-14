#pragma once

#include "AssetCooker/FAssetCookReport.h"
#include "AssetCooker/FAssetCookResult.h"

namespace Stoner::AssetCooker::Private
{

enum class EAssetCookReportCommand : Core::uint8
{
    Cook,
    Plan,
    Validate,
    ValidateCache,
    Inspect
};

struct FAssetCookReportDiagnostic
{
    Core::FString Category;
    Core::FString Stage;
    Core::FString AssetId;
    Core::TArray<Core::FString> DependencyChain;
    Core::FString SourceLocator;
    Asset::FAssetDigest TargetProfileDigest;
    Asset::FAssetDerivedKey DerivedKey;
    Asset::FAssetDigest GenerationId;
    Core::FString Field;
    Core::FString Reason;
};

struct FAssetCookReportDocument
{
    EAssetCookReportCommand Command = EAssetCookReportCommand::Cook;
    EAssetCookResultCategory Result = EAssetCookResultCategory::InternalFailure;
    Core::FString StableReason;
    FAssetCookReport Pipeline;
    bool bHasPipeline = false;
    Asset::FAssetDigest GenerationId;
    Core::uint32 Staged = 0;
    Core::uint32 Published = 0;
    Core::uint64 SourceBytes = 0;
    Core::uint64 PayloadBytes = 0;
    Core::uint64 GenerationBytes = 0;
    Core::TArray<FAssetCookReportDiagnostic> Diagnostics;
};

class FAssetCookReportCodec
{
public:
    [[nodiscard]] static Asset::EAssetResult Write(
        const FAssetCookReportDocument& Document,
        bool bNormalized,
        Core::FString& OutCanonical,
        Asset::FAssetDigest* OutDeterministicDigest = nullptr);

    [[nodiscard]] static const char* CommandToken(
        EAssetCookReportCommand Command) noexcept;
    [[nodiscard]] static const char* ResultToken(
        EAssetCookResultCategory Result) noexcept;
    [[nodiscard]] static const char* ActionToken(
        EAssetCookAction Action) noexcept;
    [[nodiscard]] static int ExitCode(
        EAssetCookResultCategory Result) noexcept;
};

} // namespace Stoner::AssetCooker::Private
