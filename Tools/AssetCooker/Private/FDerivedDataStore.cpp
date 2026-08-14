#include "FDerivedDataStore.h"

#include "Core/FPlatformFileLease.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <span>
#include <string>

namespace Stoner::AssetCooker::Private
{
namespace
{

Core::FString Join(const Core::FString& Root, std::initializer_list<std::string_view> Parts)
{
    std::filesystem::path Path(Root.ToStdString());
    for (const auto Part : Parts) Path /= std::string(Part);
    return Core::FString(Path.generic_string());
}

Core::TArray<Core::uint8> Bytes(std::string_view Text)
{
    return {Text.begin(), Text.end()};
}

Asset::FAssetDigest FailureDigest(
    const Asset::FAssetDerivedKey& Key,
    std::string_view Reason)
{
    const std::string Text = "stoner.ddc.failure.v1\nkey=" +
        Key.ToString().ToStdString() + "\nreason=" + std::string(Reason) + "\n";
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
}

FDerivedDataLookupResult LookupFailure(
    EDerivedDataLookupStatus Status,
    Asset::EAssetResult Result,
    const FDerivedDataLookupRequest& Request,
    const char* Reason)
{
    FDerivedDataLookupResult Out;
    Out.Status = Status;
    Out.Result = Result;
    Out.StableReason = Core::FString(Reason);
    Out.FailureEvidenceDigest = FailureDigest(Request.DerivedKey, Reason);
    return Out;
}

bool EnsureParents(const FDerivedDataStorePaths& Paths)
{
    return Core::FPlatformFileSystem::CreateDirectory(Core::FString(
               std::filesystem::path(Paths.EntryDirectory.ToStdString()).parent_path().generic_string())) &&
        Core::FPlatformFileSystem::CreateDirectory(Core::FString(
               std::filesystem::path(Paths.LeaseFile.ToStdString()).parent_path().generic_string())) &&
        Core::FPlatformFileSystem::CreateDirectory(Paths.StagingRoot) &&
        Core::FPlatformFileSystem::CreateDirectory(Core::FString(
            std::filesystem::path(Paths.QuarantineRoot.ToStdString())
                .parent_path().generic_string()));
}

Core::FString UniqueStage(const FDerivedDataStorePaths& Paths)
{
    static std::atomic<Core::uint64> Sequence{0};
    const auto Token = Sequence.fetch_add(1, std::memory_order_relaxed);
    const auto Tick = static_cast<Core::uint64>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return Join(Paths.StagingRoot, {
        "entry-" + std::to_string(Tick) + "-" + std::to_string(Token)});
}

bool EqualPayload(
    const FDerivedDataLookupResult& Winner,
    const Core::TArray<Core::uint8>& Payload)
{
    return Winner.Status == EDerivedDataLookupStatus::Hit &&
        Winner.Payload == Payload;
}

} // namespace

FDerivedDataStorePaths FDerivedDataStore::PathsFor(
    const Core::FString& Root,
    const Asset::FAssetDerivedKey& Key)
{
    FDerivedDataStorePaths Paths;
    if (Root.IsEmpty() || !Key.IsValid()) return Paths;
    const std::string KeyText = Key.ToString().ToStdString();
    Paths.EntryDirectory = Join(Root, {"Entries", KeyText.substr(0, 2), KeyText});
    Paths.EntryMetadata = Join(Paths.EntryDirectory, {"Entry.json"});
    Paths.EntryPayload = Join(Paths.EntryDirectory, {"Payload.sgasset"});
    Paths.LeaseFile = Join(Root, {"Locks", KeyText.substr(0, 2), KeyText + ".lock"});
    Paths.StagingRoot = Join(Root, {"Staging"});
    Paths.QuarantineRoot = Join(Root, {"Quarantine", KeyText});
    return Paths;
}

FDerivedDataLookupResult FDerivedDataStore::Lookup(
    const FDerivedDataLookupRequest& Request)
{
    if (Request.Root.IsEmpty() || !Request.DerivedKey.IsValid() ||
        Request.Evidence.Validate() != Asset::EAssetResult::Success)
        return LookupFailure(EDerivedDataLookupStatus::IoFailure,
            Asset::EAssetResult::InvalidInput, Request, "ddc.lookup.invalid-request");
    const auto Paths = PathsFor(Request.Root, Request.DerivedKey);
    if (!Core::FPlatformFileSystem::Exists(Paths.EntryDirectory))
        return LookupFailure(EDerivedDataLookupStatus::Miss,
            Asset::EAssetResult::NotFound, Request, "ddc.lookup.miss");

    Core::FPlatformFileInfo MetadataInfo;
    auto Status = Core::FPlatformFileSystem::QueryRegularFile(
        Paths.EntryMetadata, Request.EntryLimits.MaxMetadataBytes, MetadataInfo);
    if (!Status.IsSuccess())
        return LookupFailure(EDerivedDataLookupStatus::Invalid,
            Asset::EAssetResult::CorruptPayload, Request, "ddc.entry.metadata-missing-or-oversize");
    Core::TArray<Core::uint8> MetadataBytes;
    if (!Core::FPlatformFileSystem::ReadFile(Paths.EntryMetadata, MetadataBytes) ||
        MetadataBytes.size() != MetadataInfo.ByteSize)
        return LookupFailure(EDerivedDataLookupStatus::Invalid,
            Asset::EAssetResult::CorruptPayload, Request, "ddc.entry.metadata-read-failed");
    Asset::FAssetDerivedDataEntry Entry;
    if (Asset::FAssetCookContractCodec::ParseDerivedDataEntry(
            MetadataBytes, Request.EntryLimits, Entry) != Asset::EAssetResult::Success)
        return LookupFailure(EDerivedDataLookupStatus::Invalid,
            Asset::EAssetResult::CorruptPayload, Request, "ddc.entry.metadata-invalid");
    if (Entry.DerivedKey != Request.DerivedKey || Entry.Evidence != Request.Evidence ||
        Entry.RequiredExtensions != Request.RequiredExtensions)
        return LookupFailure(EDerivedDataLookupStatus::Invalid,
            Asset::EAssetResult::Conflict, Request, "ddc.entry.evidence-mismatch");

    Core::FPlatformFileInfo PayloadInfo;
    Status = Core::FPlatformFileSystem::QueryRegularFile(
        Paths.EntryPayload, Request.PayloadLimits.MaxEnvelopeBytes, PayloadInfo);
    if (!Status.IsSuccess() || PayloadInfo.ByteSize != Entry.PayloadBytes)
        return LookupFailure(EDerivedDataLookupStatus::Invalid,
            Asset::EAssetResult::CorruptPayload, Request, "ddc.entry.payload-size-mismatch");
    Core::TArray<Core::uint8> Payload;
    if (!Core::FPlatformFileSystem::ReadFile(Paths.EntryPayload, Payload) ||
        Payload.size() != PayloadInfo.ByteSize)
        return LookupFailure(EDerivedDataLookupStatus::Invalid,
            Asset::EAssetResult::CorruptPayload, Request, "ddc.entry.payload-read-failed");
    Core::TSharedPtr<const Asset::FAssetPayload> TypedPayload;
    Asset::FAssetCookedPayloadEnvelope Envelope;
    if (Asset::FAssetCookContractCodec::LoadTypedPayload(
            Payload, Request.PayloadLimits, TypedPayload, &Envelope) != Asset::EAssetResult::Success ||
        !TypedPayload || Envelope.Header.AssetId != Entry.AssetId ||
        Envelope.Header.CodecId != Entry.CodecId.ToString() ||
        Entry.CodecVersion.ToString() != Core::FString(
            std::to_string(Envelope.Header.CodecVersion)) ||
        Envelope.Header.PayloadSchemaVersion != Entry.PayloadSchemaVersion ||
        Envelope.EnvelopeDigest != Entry.EnvelopeDigest)
        return LookupFailure(EDerivedDataLookupStatus::Invalid,
            Asset::EAssetResult::CorruptPayload, Request, "ddc.entry.payload-invalid");

    FDerivedDataLookupResult Result;
    Result.Status = EDerivedDataLookupStatus::Hit;
    Result.Result = Asset::EAssetResult::Success;
    Result.StableReason = Core::FString("ddc.lookup.hit");
    Result.Entry = std::move(Entry);
    Result.Payload = std::move(Payload);
    return Result;
}

FDerivedDataInstallResult FDerivedDataStore::Install(
    const FDerivedDataLookupRequest& Request,
    const Core::TArray<Core::uint8>& Payload,
    std::chrono::milliseconds LeaseTimeout)
{
    FDerivedDataInstallResult Result;
    const auto Paths = PathsFor(Request.Root, Request.DerivedKey);
    if (Payload.empty() || !EnsureParents(Paths))
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.install.prepare-failed");
        return Result;
    }
    Asset::FAssetCookedPayloadEnvelope Envelope;
    if (Asset::FAssetCookContractCodec::ParseCookedPayload(
            Payload, Request.PayloadLimits, Envelope) != Asset::EAssetResult::Success ||
        Envelope.Header.AssetId != Request.Evidence.AssetId)
    {
        Result.Result = Asset::EAssetResult::CorruptPayload;
        Result.StableReason = Core::FString("ddc.install.payload-invalid");
        return Result;
    }
    Asset::FAssetDerivedDataEntry Entry;
    Entry.DerivedKey = Request.DerivedKey;
    Entry.Evidence = Request.Evidence;
    Entry.AssetId = Request.Evidence.AssetId;
    Entry.CodecId = Request.Evidence.CodecId;
    Entry.CodecVersion = Request.Evidence.CodecVersion;
    Entry.PayloadSchemaVersion = Request.Evidence.PayloadSchemaVersion;
    Entry.RelevantProfileDigest = Request.Evidence.RelevantProfileDigest;
    Entry.PayloadBytes = Payload.size();
    Entry.EnvelopeDigest = Envelope.EnvelopeDigest;
    Entry.RequiredExtensions = Request.RequiredExtensions;
    Core::FString Canonical;
    if (Asset::FAssetCookContractCodec::WriteDerivedDataEntry(Entry, Canonical) != Asset::EAssetResult::Success)
    {
        Result.Result = Asset::EAssetResult::InvalidInput;
        Result.StableReason = Core::FString("ddc.install.metadata-invalid");
        return Result;
    }

    const Core::FString Stage = UniqueStage(Paths);
    if (!Core::FPlatformFileSystem::CreateDirectory(Stage) ||
        !Core::FPlatformFileSystem::WriteFileDurable(Join(Stage, {"Payload.sgasset"}), Payload).IsSuccess() ||
        !Core::FPlatformFileSystem::WriteFileDurable(Join(Stage, {"Entry.json"}), Bytes(Canonical.View())).IsSuccess())
    {
        if (Core::FPlatformFileSystem::Exists(Stage))
            (void)Core::FPlatformFileSystem::RemoveTreeContained(Paths.StagingRoot, Stage);
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.install.stage-failed");
        return Result;
    }
    Core::TArray<Core::uint8> StagedMetadata;
    Core::TArray<Core::uint8> StagedPayload;
    Asset::FAssetDerivedDataEntry StagedEntry;
    Core::TSharedPtr<const Asset::FAssetPayload> StagedTypedPayload;
    if (!Core::FPlatformFileSystem::ReadFile(
            Join(Stage, {"Entry.json"}), StagedMetadata) ||
        !Core::FPlatformFileSystem::ReadFile(
            Join(Stage, {"Payload.sgasset"}), StagedPayload) ||
        StagedMetadata != Bytes(Canonical.View()) || StagedPayload != Payload ||
        Asset::FAssetCookContractCodec::ParseDerivedDataEntry(
            StagedMetadata, Request.EntryLimits, StagedEntry) !=
            Asset::EAssetResult::Success ||
        StagedEntry != Entry ||
        Asset::FAssetCookContractCodec::LoadTypedPayload(
            StagedPayload, Request.PayloadLimits, StagedTypedPayload) !=
            Asset::EAssetResult::Success || !StagedTypedPayload)
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(Paths.StagingRoot, Stage);
        Result.Result = Asset::EAssetResult::CorruptPayload;
        Result.StableReason = Core::FString("ddc.install.stage-validation-failed");
        return Result;
    }

    Core::FPlatformFileLease Lease;
    const auto LeaseStatus = Core::FPlatformFileLease::Acquire(
        Paths.LeaseFile, static_cast<Core::uint64>(LeaseTimeout.count()),
        Core::FString("stoner.asset-cooker.ddc"), Lease);
    if (!LeaseStatus.IsSuccess())
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(Paths.StagingRoot, Stage);
        Result.Result = LeaseStatus.Result == Core::EPlatformFileResult::TimedOut
            ? Asset::EAssetResult::TransientFailure : Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.install.lease-failed");
        return Result;
    }
    const auto Winner = Lookup(Request);
    if (Winner.Status == EDerivedDataLookupStatus::Hit)
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(Paths.StagingRoot, Stage);
        if (!EqualPayload(Winner, Payload))
        {
            Result.Result = Asset::EAssetResult::Conflict;
            Result.StableReason = Core::FString("ddc.install.key-collision");
            return Result;
        }
        Result.Status = EDerivedDataInstallStatus::EquivalentWinner;
        Result.Result = Asset::EAssetResult::Success;
        Result.StableReason = Core::FString("ddc.install.equivalent-winner");
        Result.Entry = Winner.Entry;
        Result.Payload = Winner.Payload;
        return Result;
    }
    if (Winner.Status == EDerivedDataLookupStatus::Invalid)
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(Paths.StagingRoot, Stage);
        Result.Result = Asset::EAssetResult::Conflict;
        Result.StableReason = Core::FString("ddc.install.invalid-final");
        return Result;
    }
    const auto Move = Core::FPlatformFileSystem::MoveDirectoryNoReplace(Stage, Paths.EntryDirectory);
    if (!Move.IsSuccess())
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(Paths.StagingRoot, Stage);
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.install.move-failed");
        return Result;
    }
    const auto Installed = Lookup(Request);
    if (Installed.Status != EDerivedDataLookupStatus::Hit || !EqualPayload(Installed, Payload))
    {
        Result.Result = Asset::EAssetResult::CorruptPayload;
        Result.StableReason = Core::FString("ddc.install.post-validate-failed");
        return Result;
    }
    Result.Status = EDerivedDataInstallStatus::Installed;
    Result.Result = Asset::EAssetResult::Success;
    Result.StableReason = Core::FString("ddc.install.installed");
    Result.Entry = Installed.Entry;
    Result.Payload = Installed.Payload;
    return Result;
}

FDerivedDataQuarantineResult FDerivedDataStore::Quarantine(
    const FDerivedDataLookupRequest& Request,
    const FDerivedDataLookupResult& InvalidEntry,
    std::chrono::milliseconds LeaseTimeout)
{
    FDerivedDataQuarantineResult Result;
    if (InvalidEntry.Status != EDerivedDataLookupStatus::Invalid ||
        !InvalidEntry.FailureEvidenceDigest.IsAvailable())
    {
        Result.Result = Asset::EAssetResult::InvalidInput;
        Result.StableReason = Core::FString("ddc.quarantine.invalid-request");
        return Result;
    }
    const auto Paths = PathsFor(Request.Root, Request.DerivedKey);
    if (!EnsureParents(Paths))
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.quarantine.prepare-failed");
        return Result;
    }
    Core::FPlatformFileLease Lease;
    const auto LeaseStatus = Core::FPlatformFileLease::Acquire(
        Paths.LeaseFile, static_cast<Core::uint64>(LeaseTimeout.count()),
        Core::FString("stoner.asset-cooker.ddc-quarantine"), Lease);
    if (!LeaseStatus.IsSuccess())
    {
        Result.Result = Asset::EAssetResult::TransientFailure;
        Result.StableReason = Core::FString("ddc.quarantine.lease-failed");
        return Result;
    }
    const auto Current = Lookup(Request);
    if (Current.Status == EDerivedDataLookupStatus::Hit || Current.Status == EDerivedDataLookupStatus::Miss)
    {
        Result.Result = Asset::EAssetResult::Success;
        Result.StableReason = Core::FString(Current.Status == EDerivedDataLookupStatus::Hit
            ? "ddc.quarantine.repaired-by-winner" : "ddc.quarantine.already-absent");
        Result.bEntryWasReplaced = Current.Status == EDerivedDataLookupStatus::Hit;
        return Result;
    }
    if (Current.Status != EDerivedDataLookupStatus::Invalid)
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.quarantine.requery-failed");
        return Result;
    }
    const Core::FString Destination = Join(Paths.QuarantineRoot,
        {Current.FailureEvidenceDigest.ToLowerHex().View()});
    if (!Core::FPlatformFileSystem::CreateDirectory(Paths.QuarantineRoot))
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.quarantine.root-failed");
        return Result;
    }
    if (Core::FPlatformFileSystem::Exists(Destination))
    {
        Result.Result = Asset::EAssetResult::Conflict;
        Result.StableReason = Core::FString("ddc.quarantine.evidence-collision");
        return Result;
    }
    const auto Move = Core::FPlatformFileSystem::MoveDirectoryNoReplace(
        Paths.EntryDirectory, Destination);
    if (!Move.IsSuccess())
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.quarantine.move-failed");
        return Result;
    }
    const std::string Failure = "{\"schema\":\"stoner.asset-ddc-failure\",\"schemaVersion\":1,\"derivedKey\":\"" +
        Request.DerivedKey.ToString().ToStdString() + "\",\"reason\":\"" +
        Current.StableReason.ToStdString() + "\"}\n";
    if (!Core::FPlatformFileSystem::WriteFileDurable(
            Join(Destination, {"Failure.json"}), Bytes(Failure)).IsSuccess())
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("ddc.quarantine.failure-evidence-write-failed");
        return Result;
    }
    Result.Result = Asset::EAssetResult::Success;
    Result.StableReason = Core::FString("ddc.quarantine.moved");
    Result.PhysicalDirectory = Destination;
    return Result;
}

} // namespace Stoner::AssetCooker::Private
