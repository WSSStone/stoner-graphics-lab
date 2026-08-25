#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FGenerationReaderLease.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TSharedPtr.h"

#include <mutex>
#include <vector>

namespace Stoner::Asset
{

struct FAssetCookedEnvelopeAuthenticationInspection
{
    FAssetDigest GenerationId;
    FAssetDigest PublicationNamespace;
    Core::uint32 Capacity = 0;
    Core::uint32 AuthenticatedEnvelopeCount = 0;
    Core::uint64 ReuseHits = 0;
    Core::uint64 AuthenticationMisses = 0;
    bool bReaderLeaseHeld = false;
};

// Records only successful envelope authentication for one immutable cooked
// generation. It never retains payload bytes, decoded assets, or runtime
// handles. Callers must hold the generation reader lease while using it.
class FAssetCookedEnvelopeAuthentication
{
public:
    ~FAssetCookedEnvelopeAuthentication();
    FAssetCookedEnvelopeAuthentication(
        const FAssetCookedEnvelopeAuthentication&) = delete;
    FAssetCookedEnvelopeAuthentication& operator=(
        const FAssetCookedEnvelopeAuthentication&) = delete;

    [[nodiscard]] static EAssetResult Create(
        const Core::FString& PublicationRoot,
        const Core::FString& LeaseCoordinationRoot,
        const FAssetDigest& GenerationId,
        Core::uint64 LeaseTimeoutMilliseconds,
        Core::uint32 Capacity,
        Core::TSharedPtr<FAssetCookedEnvelopeAuthentication>& OutContext);

    [[nodiscard]] bool MatchesBinding(
        const Core::FString& PublicationRoot,
        const FAssetDigest& GenerationId) const;
    [[nodiscard]] bool CanReuse(
        const FAssetDigest& EnvelopeDigest);
    [[nodiscard]] bool RecordAuthenticated(
        const FAssetDigest& EnvelopeDigest);
    [[nodiscard]] FAssetCookedEnvelopeAuthenticationInspection
        Inspect() const;

private:
    FAssetCookedEnvelopeAuthentication(
        FGenerationReaderLease ReaderLease,
        FAssetDigest GenerationId,
        Core::uint32 Capacity);

    mutable std::mutex Mutex_;
    FAssetDigest GenerationId_;
    FGenerationReaderLease ReaderLease_;
    Core::uint32 Capacity_ = 0;
    std::vector<FAssetDigest> AuthenticatedEnvelopes_;
    Core::uint64 ReuseHits_ = 0;
    Core::uint64 AuthenticationMisses_ = 0;
};

} // namespace Stoner::Asset
