#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetManagerConfig.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetRuntimeExecutionContext.h"
#include "Core/TArray.h"
#include "Core/TSharedPtr.h"

#include <optional>

namespace Stoner::Asset::Private
{

struct FAssetLoadKey
{
    FAssetId AssetId;
    Core::FString ExpectedType;
    EAssetManagerMode Mode = EAssetManagerMode::DevelopmentSource;
    FAssetDigest TargetDigest;
    std::optional<FAssetDigest> CookedGeneration;

    [[nodiscard]] bool operator==(const FAssetLoadKey&) const = default;
};

enum class EAssetOptionalFallbackDecision : Core::uint8
{
    NotApplicable,
    ValidatedFallback,
    Undeclared,
    Unsatisfied
};

struct FAssetOptionalFallback
{
    FAssetId Owner;
    FAssetId Dependency;
    EAssetOptionalFallbackDecision Decision =
        EAssetOptionalFallbackDecision::NotApplicable;
    Core::FString StableReason;
};

struct FAssetLoadScratchResult
{
    EAssetResult Result = EAssetResult::ProcessingFailure;
    Core::TArray<FAssetMetadata> Metadata;
    Core::TArray<Core::TSharedPtr<const FAssetPayload>> Payloads;
    Core::TArray<Core::uint64> PayloadBytes;
    Core::TArray<FAssetOptionalFallback> OptionalFallbacks;
    Core::TArray<FAssetId> FailurePath;
    FAssetDiagnosticList Diagnostics;
    bool bExtensionContractViolation = false;
};

enum class EAssetLoadOperationState : Core::uint8
{
    Created,
    ResolvingDependencies,
    Loading,
    Ready,
    Failed,
    Cancelled
};

struct FAssetRetentionCounts
{
    Core::uint64 ExternalHandles = 0;
    Core::uint64 RequestInterests = 0;
    Core::uint64 RequiredDependencies = 0;

    [[nodiscard]] bool IsUnretained() const noexcept
    {
        return ExternalHandles == 0 && RequestInterests == 0 &&
            RequiredDependencies == 0;
    }
};

} // namespace Stoner::Asset::Private
