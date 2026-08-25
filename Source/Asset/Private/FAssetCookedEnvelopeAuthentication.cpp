#include "Asset/FAssetCookedEnvelopeAuthentication.h"

#include "Core/TSharedPtr.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Stoner::Asset
{
namespace
{
void IncrementSaturated(Core::uint64& Value) noexcept
{
    if (Value != std::numeric_limits<Core::uint64>::max()) ++Value;
}
} // namespace

FAssetCookedEnvelopeAuthentication::FAssetCookedEnvelopeAuthentication(
    FGenerationReaderLease ReaderLease,
    FAssetDigest GenerationId,
    Core::uint32 Capacity)
    : GenerationId_(std::move(GenerationId)),
      ReaderLease_(std::move(ReaderLease)),
      Capacity_(Capacity)
{
}

FAssetCookedEnvelopeAuthentication::~FAssetCookedEnvelopeAuthentication()
{
    ReaderLease_.Release();
}

EAssetResult FAssetCookedEnvelopeAuthentication::Create(
    const Core::FString& PublicationRoot,
    const Core::FString& LeaseCoordinationRoot,
    const FAssetDigest& GenerationId,
    Core::uint64 LeaseTimeoutMilliseconds,
    Core::uint32 Capacity,
    Core::TSharedPtr<FAssetCookedEnvelopeAuthentication>& OutContext)
{
    OutContext.reset();
    if (PublicationRoot.IsEmpty() || LeaseCoordinationRoot.IsEmpty() ||
        !GenerationId.IsAvailable() || LeaseTimeoutMilliseconds > 600000 ||
        Capacity == 0 || Capacity > 100000)
        return EAssetResult::InvalidInput;
    FGenerationReaderLease ReaderLease;
    const EAssetResult LeaseResult = FGenerationReaderLease::Acquire(
        PublicationRoot,
        LeaseCoordinationRoot,
        GenerationId,
        LeaseTimeoutMilliseconds,
        ReaderLease);
    if (LeaseResult != EAssetResult::Success) return LeaseResult;
    OutContext = Core::TSharedPtr<FAssetCookedEnvelopeAuthentication>(
        new FAssetCookedEnvelopeAuthentication(
            std::move(ReaderLease), GenerationId, Capacity));
    return EAssetResult::Success;
}

bool FAssetCookedEnvelopeAuthentication::MatchesBinding(
    const Core::FString& PublicationRoot,
    const FAssetDigest& GenerationId) const
{
    FAssetDigest PublicationNamespace;
    if (FGenerationReaderLease::DerivePublicationNamespace(
            PublicationRoot, PublicationNamespace) != EAssetResult::Success)
        return false;
    std::lock_guard Lock(Mutex_);
    return ReaderLease_.IsHeld() && GenerationId.IsAvailable() &&
        GenerationId_ == GenerationId &&
        ReaderLease_.GetGenerationId() == GenerationId &&
        ReaderLease_.GetPublicationNamespace() == PublicationNamespace;
}

bool FAssetCookedEnvelopeAuthentication::CanReuse(
    const FAssetDigest& EnvelopeDigest)
{
    std::lock_guard Lock(Mutex_);
    const bool bFound = EnvelopeDigest.IsAvailable() &&
        std::find(
            AuthenticatedEnvelopes_.begin(),
            AuthenticatedEnvelopes_.end(),
            EnvelopeDigest) != AuthenticatedEnvelopes_.end();
    IncrementSaturated(bFound ? ReuseHits_ : AuthenticationMisses_);
    return bFound;
}

bool FAssetCookedEnvelopeAuthentication::RecordAuthenticated(
    const FAssetDigest& EnvelopeDigest)
{
    if (!EnvelopeDigest.IsAvailable()) return false;
    std::lock_guard Lock(Mutex_);
    if (std::find(
            AuthenticatedEnvelopes_.begin(),
            AuthenticatedEnvelopes_.end(),
            EnvelopeDigest) != AuthenticatedEnvelopes_.end())
        return true;
    if (AuthenticatedEnvelopes_.size() >= Capacity_) return false;
    AuthenticatedEnvelopes_.push_back(EnvelopeDigest);
    return true;
}

FAssetCookedEnvelopeAuthenticationInspection
FAssetCookedEnvelopeAuthentication::Inspect() const
{
    std::lock_guard Lock(Mutex_);
    FAssetCookedEnvelopeAuthenticationInspection Result;
    Result.GenerationId = GenerationId_;
    Result.PublicationNamespace = ReaderLease_.GetPublicationNamespace();
    Result.Capacity = Capacity_;
    Result.AuthenticatedEnvelopeCount = static_cast<Core::uint32>(
        AuthenticatedEnvelopes_.size());
    Result.ReuseHits = ReuseHits_;
    Result.AuthenticationMisses = AuthenticationMisses_;
    Result.bReaderLeaseHeld = ReaderLease_.IsHeld();
    return Result;
}

} // namespace Stoner::Asset
