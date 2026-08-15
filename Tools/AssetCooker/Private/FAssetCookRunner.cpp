#include "AssetCooker/FAssetCookRunner.h"

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"
#include "FAssetCookGraph.h"
#include "FAssetCookScheduler.h"
#include "FCookedGenerationPublisher.h"
#include "FDerivedDataStore.h"
#include "FAssetSourceCatalog.h"
#include "FCookInputSnapshot.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <span>
#include <string>

namespace Stoner::AssetCooker
{
namespace
{

FAssetCookResult Fail(
    EAssetCookResultCategory Category,
    const char* Reason,
    FAssetCookReport& OutReport)
{
    OutReport = {};
    FAssetCookResult Result;
    Result.Category = Category;
    Result.StableReason = Core::FString(Reason);
    return Result;
}

Asset::FAssetDigest DigestText(std::string_view Text)
{
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
}

const char* DependencyRole(Asset::EAssetDependencyRole Role)
{
    switch (Role)
    {
    case Asset::EAssetDependencyRole::Source: return "source";
    case Asset::EAssetDependencyRole::Build: return "build";
    case Asset::EAssetDependencyRole::Runtime: return "runtime";
    }
    return "unknown";
}

Asset::FAssetDigest VersionDigest(const Asset::FAssetVersion& Version)
{
    if (Version.SourceDigest.IsAvailable()) return Version.SourceDigest;
    if (Version.ContentDigest.IsAvailable()) return Version.ContentDigest;
    return Version.CookDigest;
}

Asset::FAssetProducerVersion CodecVersion(Core::uint32 Version)
{
    Asset::FAssetProducerVersion Result;
    (void)Asset::FAssetProducerVersion::Create(
        Core::FString(std::to_string(Version)), Result);
    return Result;
}

Asset::FAssetProducerVersion CookerVersion(
    const Asset::FAssetExtensionRegistry& Registry,
    const Asset::FAssetParticipantId& Participant)
{
    for (const auto& Capability :
         Registry.Snapshot(Asset::EAssetExtensionKind::Cooker))
        if (Capability.Participant == Participant)
            return Capability.ProducerVersion;
    return {};
}

EAssetCookResultCategory GraphCategory(Asset::EAssetResult Result)
{
    if (Result == Asset::EAssetResult::TransientFailure)
        return EAssetCookResultCategory::SourceChanged;
    return EAssetCookResultCategory::GraphFailure;
}

struct FPreparedCookNode
{
    Asset::FAssetParticipantId CookerId;
    Asset::FAssetDerivedKeyEvidence Evidence;
    Asset::FAssetDerivedKey DerivedKey;
    Asset::FAssetCookedTargetDecision TargetDecision;
    bool bReused = false;
    bool bInvalidated = false;
    bool bQuarantined = false;
    Core::FString StableReason = Core::FString("ddc.lookup.not-run");
};

Asset::EAssetResult PrepareCookNode(
    const Private::FAssetCookGraphPlan& Plan,
    const Private::FAssetCookGraphNode& Node,
    const Asset::FAssetTargetProfileEvidence& Profile,
    const Asset::FAssetExtensionRegistry& Registry,
    FPreparedCookNode& Out)
{
    Out = {};
    Asset::EAssetCookedFamily Family;
    if (Asset::GetAssetCookedFamily(Node.Metadata.Id.GetAssetType(), Family) !=
            Asset::EAssetResult::Success ||
        Asset::GetAssetCookedParticipant(
            Family, Asset::EAssetExtensionKind::Cooker, Out.CookerId) !=
            Asset::EAssetResult::Success)
        return Asset::EAssetResult::Unsupported;
    const auto CookerLease = Registry.Acquire(
        Asset::EAssetExtensionKind::Cooker, Out.CookerId);
    const auto Cooker = CookerLease.Get<Asset::IAssetCooker>();
    Asset::FAssetProfileProjectionEvidence Projection;
    if (!Cooker || Cooker->GetRelevantProfileEvidence(Profile, Projection) !=
            Asset::EAssetResult::Success)
        return Asset::EAssetResult::InvalidInput;
    if (!Node.Payload || Asset::ResolveAssetCookedTargetDecision(
            Family, *Node.Payload, Profile.Profile,
            Out.TargetDecision) != Asset::EAssetResult::Success)
        return Asset::EAssetResult::Unsupported;
    Asset::FAssetCookedPayloadHeader PayloadContract;
    if (Asset::FAssetCookContractCodec::DescribeTypedPayload(
            *Node.Payload, PayloadContract) != Asset::EAssetResult::Success ||
        PayloadContract.AssetId != Node.Metadata.Id)
        return Asset::EAssetResult::TypeMismatch;

    auto& Evidence = Out.Evidence;
    Evidence.AssetId = Node.Metadata.Id;
    Evidence.SourceVersion = VersionDigest(Node.Metadata.Version);
    Evidence.SourceManifest.push_back({Node.Metadata.Source, Evidence.SourceVersion});
    for (const Core::uint32 DependencyIndex : Node.Dependencies)
    {
        const auto& DependencyNode = Plan.Nodes[DependencyIndex];
        Asset::EAssetDependencyRole Role = Asset::EAssetDependencyRole::Build;
        const auto Declaration = std::find_if(
            Node.Metadata.Dependencies.begin(), Node.Metadata.Dependencies.end(),
            [&DependencyNode](const auto& Value)
            { return Value.TargetId == DependencyNode.Metadata.Id; });
        if (Declaration != Node.Metadata.Dependencies.end()) Role = Declaration->Role;
        Evidence.Dependencies.push_back({
            DependencyNode.Metadata.Id, DependencyNode.Metadata.Version, Role});
    }
    std::sort(Evidence.Dependencies.begin(), Evidence.Dependencies.end(),
        [](const auto& Left, const auto& Right)
        {
            if (Left.Role != Right.Role)
                return static_cast<Core::uint8>(Left.Role) <
                    static_cast<Core::uint8>(Right.Role);
            return Left.Id < Right.Id;
        });
    Evidence.ImporterId = Node.Metadata.Producer;
    Evidence.ImporterVersion = Node.Metadata.ProducerVersion;
    Evidence.CookerId = Out.CookerId;
    Evidence.CookerVersion = CookerVersion(Registry, Out.CookerId);
    if (Asset::FAssetParticipantId::Create(
            PayloadContract.CodecId, Evidence.CodecId) != Asset::EAssetResult::Success)
        return Asset::EAssetResult::InvalidInput;
    Evidence.CodecVersion = CodecVersion(PayloadContract.CodecVersion);
    Evidence.PayloadSchemaVersion = PayloadContract.PayloadSchemaVersion;
    Evidence.EffectiveSettingsDigest = Projection.EffectiveSettingsDigest;
    Evidence.RelevantProfileDigest = Projection.RelevantProfileDigest;
    return Asset::FAssetCookContractCodec::BuildDerivedKey(
        Evidence, Out.DerivedKey);
}

Private::FDerivedDataLookupRequest MakeLookupRequest(
    const FAssetCookRequest& Request,
    const Asset::FAssetTargetProfileEvidence& Profile,
    const FPreparedCookNode& Node)
{
    Private::FDerivedDataLookupRequest Lookup;
    Lookup.Root = Request.DerivedDataRoot;
    Lookup.DerivedKey = Node.DerivedKey;
    Lookup.Evidence = Node.Evidence;
    Lookup.RequiredExtensions = Profile.Profile.RequiredExtensions;
    std::sort(Lookup.RequiredExtensions.begin(), Lookup.RequiredExtensions.end());
    Lookup.PayloadLimits.MaxEnvelopeBytes = Profile.Profile.Limits.MaxPayloadBytes;
    Lookup.PayloadLimits.MaxBodyBytes = Profile.Profile.Limits.MaxPayloadBytes;
    return Lookup;
}

} // namespace

FAssetCookResult FAssetCookRunner::Run(
    const FAssetCookRequest& Request,
    FAssetCookReport& OutReport)
{
    OutReport = {};
    if (Request.Validate() != Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::InvalidArguments,
            "asset-cooker.request.invalid", OutReport);

    Core::TArray<Core::uint8> ProfileBytes;
    if (!Core::FPlatformFileSystem::ReadFile(
            Request.TargetProfilePath, ProfileBytes))
        return Fail(EAssetCookResultCategory::InvalidProfile,
            "asset-cooker.profile.unreadable", OutReport);
    Asset::FAssetTargetProfileEvidence ParsedProfile;
    if (Asset::FAssetCookContractCodec::ParseTargetProfile(
            ProfileBytes, ParsedProfile) != Asset::EAssetResult::Success ||
        ParsedProfile != Request.TargetProfile)
        return Fail(EAssetCookResultCategory::InvalidProfile,
            "asset-cooker.profile.mismatch", OutReport);

    Private::FAssetSourceCatalogResult Catalog;
    const Asset::EAssetResult Discovery =
        Private::FAssetSourceCatalog::Discover(Request, Catalog);
    if (Discovery != Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::DiscoveryFailure,
            "asset-cooker.discovery.failed", OutReport);

    Private::FAssetCookGraphLimits GraphLimits;
    GraphLimits.MaxAssets = ParsedProfile.Profile.Limits.MaxAssets;
    GraphLimits.MaxDependencyEdges =
        ParsedProfile.Profile.Limits.MaxDependencyEdges;
    GraphLimits.MaxDependencyDepth =
        ParsedProfile.Profile.Limits.MaxDependencyDepth;
    Private::FAssetCookGraphPlan Plan;
    const Asset::EAssetResult Graph = Private::FAssetCookGraph::Build(
        Catalog.Outputs, Request.SelectionMode, Request.ExplicitRoots,
        GraphLimits, Plan);
    if (Graph != Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::GraphFailure,
            "asset-cooker.graph.failed", OutReport);

    Private::FCookInputSnapshot Snapshot;
    const Asset::EAssetResult Pinned = Private::FCookInputSnapshotBuilder::Pin(
        Catalog.SnapshotSources,
        ParsedProfile.Profile.Limits.MaxSourceBytes,
        ParsedProfile.Profile.Limits.MaxAggregateBytes,
        Snapshot);
    if (Pinned != Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::DiscoveryFailure,
            "asset-cooker.snapshot.failed", OutReport);

    FAssetCookReport Report;
    Report.EffectiveProfileDigest = ParsedProfile.EffectiveProfileDigest;
    Report.SnapshotDigest = Snapshot.SnapshotDigest;
    Report.Counts.DiscoveredSources =
        static_cast<Core::uint32>(Catalog.Sources.size());
    Report.Counts.SelectedRoots = static_cast<Core::uint32>(Plan.Roots.size());
    Report.Counts.ReachableAssets = static_cast<Core::uint32>(Plan.Nodes.size());
    Report.Counts.ReuseIneligible = static_cast<Core::uint32>(Plan.Nodes.size());
    Report.Assets.reserve(Plan.Nodes.size());
    for (const auto& Node : Plan.Nodes)
    {
        FAssetCookAssetReport AssetReport;
        AssetReport.PlanIndex = Node.PlanIndex;
        AssetReport.AssetId = Node.Metadata.Id;
        AssetReport.Decision = EAssetCookDecision::Planned;
        AssetReport.StableReason = Core::FString("clean-cook");
        Report.Assets.push_back(std::move(AssetReport));
    }

    Asset::FAssetExtensionRegistry Registry;
    Asset::FAssetCookedExtensionRegistrations Registrations;
    if (Asset::RegisterCookedAssetExtensions(Registry, Registrations) !=
        Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::InternalFailure,
            "asset-cooker.extensions.failed", OutReport);
    const auto SharedProfile =
        Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(ParsedProfile);

    Core::TArray<FPreparedCookNode> Prepared(Plan.Nodes.size());
    for (const auto& Node : Plan.Nodes)
        if (PrepareCookNode(Plan, Node, ParsedProfile, Registry,
                Prepared[Node.PlanIndex]) != Asset::EAssetResult::Success)
            return Fail(EAssetCookResultCategory::CookFailure,
                "asset-cooker.derived-evidence.failed", OutReport);

    for (const auto& Node : Plan.Nodes)
    {
        const auto& PreparedNode = Prepared[Node.PlanIndex];
        auto& AssetReport = Report.Assets[Node.PlanIndex];
        AssetReport.DerivedKey = PreparedNode.DerivedKey;
        AssetReport.EffectiveSettingsDigest =
            PreparedNode.Evidence.EffectiveSettingsDigest;
        AssetReport.RelevantProfileDigest =
            PreparedNode.Evidence.RelevantProfileDigest;
        AssetReport.TargetDecision = PreparedNode.TargetDecision.Selection;
        AssetReport.bUsedFallback =
            PreparedNode.TargetDecision.bUsedFallback;
        for (const auto& Source : PreparedNode.Evidence.SourceManifest)
            AssetReport.SourceEvidence.emplace_back(
                Source.Locator.ToString().ToStdString() + "@" +
                Source.Version.ToLowerHex().ToStdString());
        for (const auto& Dependency : PreparedNode.Evidence.Dependencies)
            AssetReport.DependencyEvidence.emplace_back(
                std::string(DependencyRole(Dependency.Role)) + ":" +
                Dependency.Id.ToString().ToStdString() + "@" +
                VersionDigest(Dependency.Version).ToLowerHex().ToStdString());
    }

    if (Request.Mode == EAssetCookRunMode::PlanOnly)
    {
        Report.Counts.ReuseEligible = static_cast<Core::uint32>(Plan.Nodes.size());
        Report.Counts.ReuseIneligible = 0;
        for (const auto& Node : Plan.Nodes)
        {
            const auto& PreparedNode = Prepared[Node.PlanIndex];
            auto& AssetReport = Report.Assets[Node.PlanIndex];
            AssetReport.bReuseEligible = true;
            const auto Lookup = Private::FDerivedDataStore::Lookup(
                MakeLookupRequest(Request, ParsedProfile, PreparedNode));
            if (Lookup.Status == Private::EDerivedDataLookupStatus::Hit)
            {
                AssetReport.Decision = EAssetCookDecision::Reused;
                AssetReport.Action = EAssetCookAction::Hit;
                AssetReport.StableReason = Lookup.StableReason;
                ++Report.Counts.Reused;
            }
            else if (Lookup.Status == Private::EDerivedDataLookupStatus::Miss)
            {
                AssetReport.Action = EAssetCookAction::Miss;
                AssetReport.StableReason = Lookup.StableReason;
                ++Report.Counts.CacheMisses;
            }
            else if (Lookup.Status == Private::EDerivedDataLookupStatus::Invalid)
            {
                AssetReport.Action = EAssetCookAction::Invalidate;
                AssetReport.StableReason = Core::FString(
                    "ddc.lookup.corrupt-would-rebuild");
                ++Report.Counts.CacheMisses;
                ++Report.Counts.Invalidated;
            }
            else
            {
                AssetReport.Decision = EAssetCookDecision::Failed;
                AssetReport.Action = EAssetCookAction::Fail;
                AssetReport.StableReason = Lookup.StableReason;
                ++Report.Counts.Failed;
                OutReport = std::move(Report);
                FAssetCookResult Failed;
                Failed.Category = EAssetCookResultCategory::CacheFailure;
                Failed.StableReason = Core::FString("asset-cooker.plan.cache-failed");
                return Failed;
            }
        }
        if (Report.Validate() != Asset::EAssetResult::Success)
            return Fail(EAssetCookResultCategory::InternalFailure,
                "asset-cooker.report.invalid", OutReport);
        OutReport = std::move(Report);
        FAssetCookResult Result;
        Result.Category = EAssetCookResultCategory::Success;
        Result.StableReason = Core::FString("asset-cooker.plan.success");
        return Result;
    }
    const bool bCacheEligible =
        Request.CachePolicy != EAssetCookCachePolicy::IgnoreExisting;
    Report.Counts.ReuseEligible = bCacheEligible
        ? static_cast<Core::uint32>(Plan.Nodes.size()) : 0;
    Report.Counts.ReuseIneligible = bCacheEligible
        ? 0 : static_cast<Core::uint32>(Plan.Nodes.size());
    for (auto& AssetReport : Report.Assets)
    {
        AssetReport.bReuseEligible = bCacheEligible;
        AssetReport.StableReason = Core::FString(bCacheEligible
            ? "ddc.lookup.pending" : "ddc.lookup.bypassed-clean");
        AssetReport.Action = bCacheEligible
            ? EAssetCookAction::Miss : EAssetCookAction::Ineligible;
    }

    std::atomic<bool> bCacheFailure{false};
    Private::FAssetCookScheduleOutput Scheduled;
    const Asset::EAssetResult Schedule = Private::FAssetCookScheduler::Execute(
        Plan, Request.WorkerCount,
        [&Registry, &Catalog, &Snapshot, &ParsedProfile, &SharedProfile,
         &Request, &Prepared, &bCacheFailure](
            const Private::FAssetCookGraphNode& Node,
            const Core::TArray<Private::FAssetCookScheduledResult>& Results)
        {
            (void)Results;
            auto& State = Prepared[Node.PlanIndex];
            if (Private::FCookInputSnapshotBuilder::Revalidate(
                    Snapshot, ParsedProfile.Profile.Limits.MaxSourceBytes,
                    Catalog.Revalidate) != Asset::EAssetResult::Success)
                return Private::FAssetCookScheduledResult{
                    Asset::EAssetResult::TransientFailure, {}};

            const auto LookupRequest = MakeLookupRequest(
                Request, ParsedProfile, State);
            if (Request.CachePolicy != EAssetCookCachePolicy::IgnoreExisting)
            {
                auto Lookup = Private::FDerivedDataStore::Lookup(LookupRequest);
                if (Lookup.Status == Private::EDerivedDataLookupStatus::Hit)
                {
                    State.bReused = true;
                    State.StableReason = Lookup.StableReason;
                    return Private::FAssetCookScheduledResult{
                        Asset::EAssetResult::Success, std::move(Lookup.Payload)};
                }
                if (Request.CachePolicy == EAssetCookCachePolicy::StrictValidate)
                {
                    State.StableReason = Lookup.StableReason;
                    bCacheFailure.store(true, std::memory_order_relaxed);
                    return Private::FAssetCookScheduledResult{
                        Asset::EAssetResult::CorruptPayload, {}};
                }
                if (Lookup.Status == Private::EDerivedDataLookupStatus::Invalid)
                {
                    State.bInvalidated = true;
                    const auto Quarantine = Private::FDerivedDataStore::Quarantine(
                        LookupRequest, Lookup, Request.LeaseTimeout);
                    if (Quarantine.Result != Asset::EAssetResult::Success)
                    {
                        State.StableReason = Quarantine.StableReason;
                        bCacheFailure.store(true, std::memory_order_relaxed);
                        return Private::FAssetCookScheduledResult{
                            Asset::EAssetResult::CorruptPayload, {}};
                    }
                    State.bQuarantined = Quarantine.bEntryQuarantined;
                    if (Quarantine.bEntryWasReplaced)
                    {
                        Lookup = Private::FDerivedDataStore::Lookup(LookupRequest);
                        if (Lookup.Status == Private::EDerivedDataLookupStatus::Hit)
                        {
                            State.bInvalidated = false;
                            State.bQuarantined = false;
                            State.bReused = true;
                            State.StableReason = Core::FString("ddc.lookup.repaired-winner");
                            return Private::FAssetCookScheduledResult{
                                Asset::EAssetResult::Success, std::move(Lookup.Payload)};
                        }
                    }
                    State.StableReason = Core::FString("ddc.lookup.corrupt-rebuild");
                }
                else if (Lookup.Status == Private::EDerivedDataLookupStatus::Miss)
                    State.StableReason = Lookup.StableReason;
                else
                {
                    State.StableReason = Lookup.StableReason;
                    bCacheFailure.store(true, std::memory_order_relaxed);
                    return Private::FAssetCookScheduledResult{
                        Asset::EAssetResult::ProcessingFailure, {}};
                }
            }
            Asset::FAssetCookRequest CookRequest;
            CookRequest.Metadata = Node.Metadata;
            CookRequest.Payload = Node.Payload;
            CookRequest.TargetProfile = ParsedProfile.Profile.DisplayName;
            CookRequest.TargetProfileEvidence = SharedProfile;
            Asset::FAssetCookResult Cooked = Asset::FAssetDispatch::Cook(
                Registry, State.CookerId, CookRequest);
            if (Cooked.Result != Asset::EAssetResult::Success)
                return Private::FAssetCookScheduledResult{
                    Cooked.Result, std::move(Cooked.Artifact)};

            auto Installed = Private::FDerivedDataStore::Install(
                LookupRequest, Cooked.Artifact, Request.LeaseTimeout);
            if (Installed.Result != Asset::EAssetResult::Success &&
                Request.CachePolicy == EAssetCookCachePolicy::IgnoreExisting)
            {
                const auto Existing = Private::FDerivedDataStore::Lookup(LookupRequest);
                if (Existing.Status == Private::EDerivedDataLookupStatus::Invalid)
                {
                    const auto Quarantine = Private::FDerivedDataStore::Quarantine(
                        LookupRequest, Existing, Request.LeaseTimeout);
                    if (Quarantine.Result == Asset::EAssetResult::Success)
                        Installed = Private::FDerivedDataStore::Install(
                            LookupRequest, Cooked.Artifact, Request.LeaseTimeout);
                }
            }
            if (Installed.Result != Asset::EAssetResult::Success)
            {
                State.StableReason = Installed.StableReason;
                bCacheFailure.store(true, std::memory_order_relaxed);
                return Private::FAssetCookScheduledResult{
                    Asset::EAssetResult::ProcessingFailure, {}};
            }
            if (State.bInvalidated)
                State.StableReason = Core::FString("ddc.lookup.corrupt-rebuilt");
            else if (Request.CachePolicy == EAssetCookCachePolicy::IgnoreExisting)
                State.StableReason = Core::FString("ddc.lookup.bypassed-clean");
            else
                State.StableReason = Core::FString("ddc.lookup.miss-cooked");
            return Private::FAssetCookScheduledResult{
                Asset::EAssetResult::Success, std::move(Installed.Payload)};
        },
        Scheduled);
    if (Schedule != Asset::EAssetResult::Success)
        return Fail(bCacheFailure.load(std::memory_order_relaxed)
                ? EAssetCookResultCategory::CacheFailure : GraphCategory(Schedule),
            bCacheFailure.load(std::memory_order_relaxed)
                ? "asset-cooker.cache.failed"
                : Schedule == Asset::EAssetResult::TransientFailure
                ? "asset-cooker.source.changed"
                : "asset-cooker.cook.failed",
            OutReport);

    Asset::FAssetCookManifest Manifest;
    Manifest.TargetProfile = ParsedProfile;
    Manifest.Selection.Mode = Request.SelectionMode;
    Manifest.Selection.Roots = Plan.Roots;
    for (Core::usize Index = 0; Index < Request.SourceRoots.size(); ++Index)
        Manifest.Selection.SourceScopes.push_back(
            Core::FString("scope/" + std::to_string(Index)));
    Manifest.Selection.DiscoveryRulesVersion =
        DigestText("stoner.asset-cooker.discovery.v1");
    Manifest.SnapshotDigest = Snapshot.SnapshotDigest;
    Manifest.LimitsDigest = DigestText(
        ParsedProfile.CanonicalEffectiveConfiguration.View());
    Manifest.RequiredExtensions = ParsedProfile.Profile.RequiredExtensions;
    std::sort(Manifest.RequiredExtensions.begin(), Manifest.RequiredExtensions.end());

    Core::TArray<FAssetCookArtifact> Artifacts;
    Artifacts.reserve(Plan.Nodes.size());
    for (const auto& Node : Plan.Nodes)
    {
        const auto& ArtifactBytes = Scheduled.Results[Node.PlanIndex].Artifact;
        Asset::FAssetCookedPayloadEnvelope Envelope;
        if (Asset::FAssetCookContractCodec::ParseCookedPayload(
                ArtifactBytes, {}, Envelope) != Asset::EAssetResult::Success ||
            Envelope.Header.AssetId != Node.Metadata.Id)
            return Fail(EAssetCookResultCategory::CookFailure,
                "asset-cooker.envelope.invalid", OutReport);

        const auto& PreparedNode = Prepared[Node.PlanIndex];
        const auto& KeyEvidence = PreparedNode.Evidence;
        const auto& DerivedKey = PreparedNode.DerivedKey;
        if (Envelope.Header.CodecId != KeyEvidence.CodecId.ToString() ||
            KeyEvidence.CodecVersion.ToString() != Core::FString(
                std::to_string(Envelope.Header.CodecVersion)) ||
            Envelope.Header.PayloadSchemaVersion != KeyEvidence.PayloadSchemaVersion)
            return Fail(EAssetCookResultCategory::CookFailure,
                "asset-cooker.envelope.contract-mismatch", OutReport);

        Asset::FAssetCookManifestRecord Record;
        Record.AssetId = Node.Metadata.Id;
        Record.AssetType = Node.Metadata.Id.GetAssetType();
        Record.SourceVersion = KeyEvidence.SourceVersion;
        Record.SourceManifest = {{
            Node.Metadata.Id, KeyEvidence.SourceVersion,
            Core::FString("primary")}};
        Record.Importer = {
            Node.Metadata.Producer, Node.Metadata.ProducerVersion};
        Record.Cooker = {PreparedNode.CookerId, KeyEvidence.CookerVersion};
        Record.Codec = {KeyEvidence.CodecId, KeyEvidence.CodecVersion};
        Record.DerivedKey = DerivedKey;
        Record.PayloadSchemaVersion = Envelope.Header.PayloadSchemaVersion;
        const std::string EnvelopeHex =
            Envelope.EnvelopeDigest.ToLowerHex().ToStdString();
        Record.PayloadLocator = Core::FString(
            "Payloads/" + EnvelopeHex.substr(0, 2) + "/" +
            EnvelopeHex + ".sgasset");
        Record.PayloadBytes = ArtifactBytes.size();
        Record.EnvelopeDigest = Envelope.EnvelopeDigest;
        for (const auto& Dependency : KeyEvidence.Dependencies)
            Record.Dependencies.push_back({
                Dependency.Id,
                Core::FString(DependencyRole(Dependency.Role)),
                VersionDigest(Dependency.Version)});
        std::sort(Record.Dependencies.begin(), Record.Dependencies.end(),
            [](const auto& Left, const auto& Right)
            {
                if (Left.Role != Right.Role) return Left.Role < Right.Role;
                return Left.AssetId < Right.AssetId;
            });
        Manifest.Records.push_back(Record);
        Artifacts.push_back({
            Node.Metadata.Id, Record.PayloadLocator,
            Envelope.EnvelopeDigest, ArtifactBytes});
        Report.Assets[Node.PlanIndex].Decision = PreparedNode.bReused
            ? EAssetCookDecision::Reused : EAssetCookDecision::Cooked;
        Report.Assets[Node.PlanIndex].Action = PreparedNode.bReused
            ? EAssetCookAction::Reuse
            : PreparedNode.bInvalidated
                ? EAssetCookAction::Rebuild : EAssetCookAction::Cook;
        Report.Assets[Node.PlanIndex].StableReason = PreparedNode.StableReason;
        Report.Assets[Node.PlanIndex].ArtifactDigest = Envelope.EnvelopeDigest;
    }
    std::sort(Manifest.Records.begin(), Manifest.Records.end(),
        [](const auto& Left, const auto& Right)
        { return Left.AssetId < Right.AssetId; });
    std::sort(Artifacts.begin(), Artifacts.end(),
        [](const auto& Left, const auto& Right)
        { return Left.RelativeLocator < Right.RelativeLocator; });

    Asset::FAssetCookManifestLimits ManifestLimits;
    ManifestLimits.MaxManifestBytes =
        ParsedProfile.Profile.Limits.MaxManifestBytes;
    ManifestLimits.MaxRecords = ParsedProfile.Profile.Limits.MaxAssets;
    ManifestLimits.MaxDependenciesPerRecord =
        std::min<Core::uint32>(
            ParsedProfile.Profile.Limits.MaxDependencyEdges, 100000U);
    Core::FString CanonicalManifest;
    if (Asset::FAssetCookContractCodec::WriteManifest(
            Manifest, ManifestLimits, CanonicalManifest) !=
        Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::CookFailure,
            "asset-cooker.manifest.failed", OutReport);

    Private::FCookedGenerationImageRequest ImageRequest;
    ImageRequest.ScratchRoot = Request.ScratchRoot;
    ImageRequest.Manifest = Manifest;
    ImageRequest.CanonicalManifest = CanonicalManifest;
    ImageRequest.Artifacts = Artifacts;
    ImageRequest.ManifestLimits = ManifestLimits;
    ImageRequest.PayloadLimits.MaxEnvelopeBytes =
        ParsedProfile.Profile.Limits.MaxPayloadBytes;
    ImageRequest.PayloadLimits.MaxBodyBytes =
        ParsedProfile.Profile.Limits.MaxPayloadBytes;
    const auto Image =
        Private::FCookedGenerationPublisher::BuildRequestImage(ImageRequest);
    if (!Image.Succeeded())
        return Fail(EAssetCookResultCategory::IoFailure,
            Image.StableReason.ToStdString().c_str(), OutReport);
    const Core::FString ImageRoot = Image.ImageRoot;

    for (const auto& State : Prepared)
    {
        if (State.bReused) ++Report.Counts.Reused;
        else ++Report.Counts.Cooked;
        if (!State.bReused &&
            Request.CachePolicy == EAssetCookCachePolicy::Incremental)
            ++Report.Counts.CacheMisses;
        if (State.bInvalidated) ++Report.Counts.Invalidated;
        if (State.bQuarantined) ++Report.Counts.Quarantined;
        if (State.bInvalidated && !State.bReused) ++Report.Counts.Rebuilt;
    }
    Report.Telemetry.WorkerCount = Request.WorkerCount;
    Report.GenerationDigest = Manifest.GenerationId;
    if (Report.Validate() != Asset::EAssetResult::Success)
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(
            Request.ScratchRoot, ImageRoot);
        return Fail(EAssetCookResultCategory::InternalFailure,
            "asset-cooker.report.invalid", OutReport);
    }

    Private::FCookedGenerationPublicationRequest PublicationRequest;
    PublicationRequest.RequestImageRoot = ImageRoot;
    PublicationRequest.OutputRoot = Request.OutputRoot;
    PublicationRequest.Manifest = Manifest;
    PublicationRequest.CanonicalManifest = CanonicalManifest;
    PublicationRequest.ManifestLimits = ManifestLimits;
    PublicationRequest.PayloadLimits.MaxEnvelopeBytes =
        ParsedProfile.Profile.Limits.MaxPayloadBytes;
    PublicationRequest.PayloadLimits.MaxBodyBytes =
        ParsedProfile.Profile.Limits.MaxPayloadBytes;
    PublicationRequest.LeaseTimeout = Request.LeaseTimeout;
    PublicationRequest.RevalidateInputs = [&Snapshot, &ParsedProfile, &Catalog]()
    {
        return Private::FCookInputSnapshotBuilder::Revalidate(
            Snapshot, ParsedProfile.Profile.Limits.MaxSourceBytes,
            Catalog.Revalidate);
    };
    const auto Publication =
        Private::FCookedGenerationPublisher::Publish(PublicationRequest);
    if (!Publication.Succeeded())
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(
            Request.ScratchRoot, ImageRoot);
        return Fail(Publication.Category,
            Publication.StableReason.ToStdString().c_str(), OutReport);
    }

    FAssetCookResult Result;
    Result.Category = EAssetCookResultCategory::Success;
    Result.StableReason = Core::FString(
        Request.CachePolicy == EAssetCookCachePolicy::IgnoreExisting
            ? "asset-cooker.clean.success"
            : "asset-cooker.incremental.success");
    Result.GenerationImageRoot = Publication.GenerationDirectory;
    Result.Manifest = std::move(Manifest);
    Result.CanonicalManifest = std::move(CanonicalManifest);
    Result.Artifacts = std::move(Artifacts);
    (void)Core::FPlatformFileSystem::RemoveTreeContained(
        Request.ScratchRoot, ImageRoot);
    OutReport = std::move(Report);
    return Result;
}

} // namespace Stoner::AssetCooker
