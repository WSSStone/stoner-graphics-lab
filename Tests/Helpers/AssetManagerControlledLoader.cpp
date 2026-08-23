#include "AssetManagerControlledLoader.h"

#include "../AssetManagerTestSupport.h"

#include <chrono>
#include <span>

namespace Stoner::Tests
{
namespace
{
Asset::FAssetParticipantId Participant()
{
    Asset::FAssetParticipantId Result;
    (void)Asset::FAssetParticipantId::Create(
        Core::FString("importer.runtime-controlled"), Result);
    return Result;
}

Asset::FAssetProducerVersion Version()
{
    Asset::FAssetProducerVersion Result;
    (void)Asset::FAssetProducerVersion::Create(
        Core::FString("1.0.0"), Result);
    return Result;
}
} // namespace

bool FAssetManagerControlledState::WaitUntilStarted(
    Core::uint64 TimeoutMilliseconds)
{
    std::unique_lock Lock(Mutex_);
    return Wake_.wait_for(Lock,
        std::chrono::milliseconds(TimeoutMilliseconds),
        [this] { return Started_; });
}

void FAssetManagerControlledState::Release()
{
    {
        std::lock_guard Lock(Mutex_);
        Released_ = true;
    }
    Wake_.notify_all();
}

int FAssetManagerControlledState::GetCalls() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return Calls_;
}

FAssetManagerControlledLoader::FAssetManagerControlledLoader(
    Asset::FAssetId OutputId,
    Core::TSharedPtr<FAssetManagerControlledState> State,
    bool Conforming)
    : OutputId_(std::move(OutputId)), State_(std::move(State)),
      Conforming_(Conforming)
{
}

Asset::FAssetExtensionCapability
FAssetManagerControlledLoader::GetCapability() const
{
    Asset::FAssetExtensionCapability Result;
    Result.Kind = Asset::EAssetExtensionKind::Importer;
    Result.Participant = Participant();
    Result.ProducerVersion = Version();
    Result.Priority = 200;
    Result.FormatHints = {Core::FString("runtime-test")};
    Result.ProbeByteLimit = 64;
    Result.bRuntimeCompatible = true;
    return Result;
}

Asset::FAssetProbeResult FAssetManagerControlledLoader::Probe(
    const Asset::FAssetSourceDescriptor& Descriptor,
    std::span<const Core::uint8> Prefix)
{
    (void)Descriptor;
    return Prefix.empty()
        ? Asset::FAssetProbeResult{Asset::EAssetResult::MalformedSource, 0, {}}
        : Asset::FAssetProbeResult{
              Asset::EAssetResult::Success, 100, Core::FString("controlled")};
}

Asset::EAssetResult FAssetManagerControlledLoader::Import(
    const Asset::FAssetSourceDescriptor& Descriptor,
    const Asset::FAssetSourceLease& Source,
    Core::TArray<Asset::FAssetImportOutput>& OutOutputs)
{
    return Import({Descriptor, Source, {}, {}, {}}, OutOutputs);
}

Asset::EAssetResult FAssetManagerControlledLoader::Import(
    const Asset::FAssetImportRequest& Request,
    Core::TArray<Asset::FAssetImportOutput>& OutOutputs)
{
    {
        std::lock_guard Lock(State_->Mutex_);
        ++State_->Calls_;
        State_->Started_ = true;
    }
    State_->Wake_.notify_all();
    for (;;)
    {
        std::unique_lock Lock(State_->Mutex_);
        if (State_->Released_) break;
        State_->Wake_.wait_for(Lock, std::chrono::milliseconds(2));
        if (Conforming_ && Request.RuntimeContext &&
            Request.RuntimeContext->ShouldStop())
            return Asset::EAssetResult::Cancelled;
        if (!Conforming_ && Request.RuntimeContext &&
            std::chrono::steady_clock::now() >=
                Request.RuntimeContext->Deadline + std::chrono::milliseconds(5))
            break;
    }
    if (Conforming_ && Request.RuntimeContext &&
        Request.RuntimeContext->ShouldStop())
        return Asset::EAssetResult::Cancelled;

    Core::TArray<Core::uint8> Bytes;
    if (Request.Source.ReadBounded(1024, Request.Descriptor.Size, Bytes) !=
        Asset::EAssetResult::Success)
        return Asset::EAssetResult::MalformedSource;
    Asset::FAssetMetadata Metadata;
    Metadata.Id = OutputId_;
    Metadata.Source = Request.Descriptor.Location;
    Metadata.Producer = Participant();
    Metadata.ProducerVersion = Version();
    Metadata.Version.SourceDigest = Asset::FAssetDigest::FromBytes(Bytes);
    Metadata.Version.ContentDigest = Metadata.Version.SourceDigest;
    Metadata.Version.Producer = Metadata.Producer;
    Metadata.Version.ProducerVersion = Metadata.ProducerVersion;
    OutOutputs = {{std::move(Metadata),
        Core::MakeShared<Asset::FRuntimeTestPayload>(
            Core::FString("controlled-payload"))}};
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::Tests
