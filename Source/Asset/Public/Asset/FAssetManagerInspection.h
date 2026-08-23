#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetManagerConfig.h"
#include "Asset/FAssetRequestHandle.h"
#include "Core/FString.h"
#include "Core/TArray.h"
#include "Core/FPlatformTypes.h"

namespace Stoner::Asset
{

struct FAssetRequestInspectionRecord
{
    FAssetRequestSnapshot Request;
    FAssetId AssetId;
    Core::FString ExpectedType;
    bool bCacheHit = false;
    bool bCoalesced = false;
};

struct FAssetOperationInspectionRecord
{
    FAssetId AssetId;
    Core::FString ExpectedType;
    EAssetManagerMode Mode = EAssetManagerMode::DevelopmentSource;
    FAssetDigest TargetDigest;
    FAssetDigest CookedGeneration;
    Core::uint32 CallerInterests = 0;
    EAssetResult Result = EAssetResult::NotReady;
    Core::TArray<FAssetId> FailurePath;
};

struct FAssetCacheInspectionRecord
{
    FAssetId AssetId;
    Core::FString ExpectedType;
    Core::uint64 PayloadBytes = 0;
    Core::uint64 ExternalHandles = 0;
    Core::uint64 RequestInterests = 0;
    Core::uint64 RequiredDependencies = 0;
};

struct FAssetManagerInspection
{
    EAssetManagerMode Mode = EAssetManagerMode::DevelopmentSource;
    FAssetDigest BoundGeneration;
    FAssetManagerLimits Limits;
    Core::uint32 AcceptedRequests = 0;
    Core::uint32 ReadyRequests = 0;
    Core::uint32 FailedRequests = 0;
    Core::uint32 CancelledRequests = 0;
    Core::uint32 ActiveOperations = 0;
    Core::uint32 CachedAssets = 0;
    Core::uint64 CachedPayloadBytes = 0;
    Core::uint64 ExternalHandleRetentions = 0;
    Core::uint64 RequestRetentions = 0;
    Core::uint64 RequiredDependencyRetentions = 0;
    Core::uint64 ExtensionContractViolations = 0;
    Core::uint64 ResolverExecutions = 0;
    Core::uint64 ImporterExecutions = 0;
    Core::uint64 AuthoringDecoderExecutions = 0;
    Core::uint64 SourceFallbackExecutions = 0;
    Core::uint64 StrictLoaderExecutions = 0;
    Core::uint32 CompletionReservations = 0;
    Core::uint32 QueuedCompletions = 0;
    Core::TArray<FAssetRequestInspectionRecord> Requests;
    Core::TArray<FAssetOperationInspectionRecord> Operations;
    Core::TArray<FAssetCacheInspectionRecord> Cache;
    bool bShuttingDown = false;
    bool bInspectionTruncated = false;
};

} // namespace Stoner::Asset
