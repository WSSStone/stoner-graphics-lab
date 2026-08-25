#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetTargetProfile.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TSharedPtr.h"

namespace Stoner::Asset
{

class FAssetCookedEnvelopeAuthentication;

enum class EAssetManagerMode : Core::uint8
{
    DevelopmentSource,
    StrictCooked
};

struct FAssetManagerLimits
{
    Core::uint32 MaxKnownAssets = 100000;
    Core::uint32 MaxDependencyEdges = 1000000;
    Core::uint32 MaxDependencyDepth = 256;
    Core::uint64 MaxPayloadBytes = 1024ULL * 1024ULL * 1024ULL;
    Core::uint64 MaxAggregatePayloadBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    Core::uint32 MaxDiagnostics = 4096;
    Core::uint32 MaxRequests = 65536;
    Core::uint32 MaxQueuedWork = 65536;
    Core::uint32 MaxCompletions = 65536;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return MaxKnownAssets > 0 && MaxDependencyEdges > 0 &&
            MaxDependencyDepth > 0 && MaxPayloadBytes > 0 &&
            MaxAggregatePayloadBytes >= MaxPayloadBytes &&
            MaxDiagnostics > 0 && MaxRequests > 0 && MaxQueuedWork > 0 &&
            MaxCompletions > 0;
    }
};

struct FAssetManagerConfig
{
    EAssetManagerMode Mode = EAssetManagerMode::DevelopmentSource;
    Core::TSharedPtr<FAssetExtensionRegistry> ExtensionRegistry;
    Core::FString SourceRoot;
    Core::FString PublicationRoot;
    Core::FString LeaseCoordinationRoot;
    Core::TSharedPtr<const FAssetTargetProfileEvidence> TargetEvidence;
    Core::TSharedPtr<FAssetCookedEnvelopeAuthentication>
        CookedEnvelopeAuthentication;
    Core::uint32 WorkerCount = 4;
    FAssetManagerLimits Limits;
    Core::uint64 LeaseTimeoutMilliseconds = 5000;
    Core::uint64 ExtensionDeadlineMilliseconds = 30000;

    [[nodiscard]] EAssetResult Validate() const noexcept
    {
        if (!ExtensionRegistry || !TargetEvidence ||
            TargetEvidence->Validate() != EAssetResult::Success ||
            WorkerCount == 0 || WorkerCount > 32 || !Limits.IsValid() ||
            LeaseTimeoutMilliseconds > 600000 ||
            ExtensionDeadlineMilliseconds == 0 ||
            ExtensionDeadlineMilliseconds > 600000)
            return EAssetResult::InvalidInput;
        if (Mode == EAssetManagerMode::DevelopmentSource)
            return !SourceRoot.IsEmpty() && PublicationRoot.IsEmpty() &&
                LeaseCoordinationRoot.IsEmpty() &&
                !CookedEnvelopeAuthentication
                ? EAssetResult::Success
                : EAssetResult::InvalidInput;
        if (Mode == EAssetManagerMode::StrictCooked)
            return SourceRoot.IsEmpty() && !PublicationRoot.IsEmpty() &&
                !LeaseCoordinationRoot.IsEmpty()
                ? EAssetResult::Success
                : EAssetResult::InvalidInput;
        return EAssetResult::InvalidInput;
    }
};

} // namespace Stoner::Asset
