#include "AssetCooker/FAssetCookRunner.h"

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"
#include "FAssetCookGraph.h"
#include "FAssetCookerSelection.h"
#include "FAssetCookScheduler.h"
#include "FCookedGenerationPublisher.h"
#include "FDerivedDataStore.h"
#include "FAssetSourceCatalog.h"
#include "FCookInputSnapshot.h"
#include "FMetalShaderCooker.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
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
    Asset::FAssetCookedPayloadHeader OutputContract;
    Core::TSharedPtr<const Asset::FAssetCookParameters> Parameters;
    bool bReused = false;
    bool bInvalidated = false;
    bool bQuarantined = false;
    Core::FString StableReason = Core::FString("ddc.lookup.not-run");
};

bool IsMetalShaderSource(
    const Private::FAssetCookGraphNode& Node,
    const Asset::FAssetTargetProfile& Profile)
{
    const auto Payload =
        std::dynamic_pointer_cast<const Asset::FShaderPayloadAsset>(Node.Payload);
    return Payload &&
        Payload->GetBackend() == Asset::EShaderBackendFamily::Vulkan &&
        Payload->GetFormat() == Asset::EShaderPayloadFormat::SPIRV &&
        Profile.Platform == Asset::EAssetTargetPlatform::MacOS &&
        Profile.GraphicsBackend == Asset::EAssetGraphicsBackend::Metal;
}

const Asset::FShaderAsset* FindShaderProgram(
    const Private::FAssetCookGraphPlan& Plan,
    const Asset::FAssetId& PayloadId)
{
    for (const auto& Candidate : Plan.Nodes)
    {
        const auto Program =
            std::dynamic_pointer_cast<const Asset::FShaderAsset>(
                Candidate.Payload);
        if (!Program) continue;
        for (const auto& Variant : Program->GetDesc().Variants)
            for (const auto& Reference : Variant.Payloads)
                if (Reference.Payload.GetId() &&
                    *Reference.Payload.GetId() == PayloadId)
                    return Program.get();
    }
    return nullptr;
}

Core::FString ArchitectureToken(Asset::EAssetTargetCpuArchitecture Architecture)
{
    return Core::FString(
        Architecture == Asset::EAssetTargetCpuArchitecture::Arm64
            ? "arm64" : "x86_64");
}

Asset::EAssetResult PrepareCookNode(
    const Private::FAssetCookGraphPlan& Plan,
    const Private::FAssetCookGraphNode& Node,
    const Asset::FAssetTargetProfileEvidence& Profile,
    const Asset::FAssetExtensionRegistry& Registry,
    const Private::FMetalToolchainEvidence* MetalToolchain,
    const Core::FString& ScratchRoot,
    FPreparedCookNode& Out)
{
    Out = {};
    Asset::EAssetCookedFamily Family;
    if (Asset::GetAssetCookedFamily(Node.Metadata.Id.GetAssetType(), Family) !=
            Asset::EAssetResult::Success || !Node.Payload)
        return Asset::EAssetResult::Unsupported;
    Private::FAssetCookerSelection Selection;
    if (Private::SelectAssetCooker(
            Family, *Node.Payload, Profile, Registry, Selection) !=
        Asset::EAssetResult::Success)
        return Asset::EAssetResult::Unsupported;
    Out.CookerId = Selection.CookerId;
    Out.Parameters = Selection.Parameters;
    Out.OutputContract = Selection.OutputContract;
    Out.TargetDecision = Selection.TargetDecision;
    Asset::FAssetProfileProjectionEvidence Projection =
        Selection.ProfileProjection;
    if (IsMetalShaderSource(Node, Profile.Profile))
    {
        Out.CookerId = Private::FMetalShaderCooker::ParticipantId();
        const auto CookerLease = Registry.Acquire(
            Asset::EAssetExtensionKind::Cooker, Out.CookerId);
        const auto Cooker = CookerLease.Get<Asset::IAssetCooker>();
        if (!Cooker || Cooker->GetRelevantProfileEvidence(
                Profile, Projection) != Asset::EAssetResult::Success)
            return Asset::EAssetResult::InvalidInput;
    }
    if (Out.OutputContract.AssetId != Node.Metadata.Id)
        return Asset::EAssetResult::TypeMismatch;
    if (IsMetalShaderSource(Node, Profile.Profile))
    {
        Out.OutputContract.CodecVersion = 2;
        Out.OutputContract.PayloadSchemaVersion = 2;
    }

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
            Out.OutputContract.CodecId, Evidence.CodecId) !=
        Asset::EAssetResult::Success)
        return Asset::EAssetResult::InvalidInput;
    Evidence.CodecVersion = CodecVersion(Out.OutputContract.CodecVersion);
    Evidence.PayloadSchemaVersion = Out.OutputContract.PayloadSchemaVersion;
    Evidence.EffectiveSettingsDigest = Projection.EffectiveSettingsDigest;
    Evidence.RelevantProfileDigest = Projection.RelevantProfileDigest;
    Evidence.AdditionalEvidence = Selection.AdditionalEvidence;
    if (!Evidence.AdditionalEvidence.empty())
        Evidence.KeyFormatVersion =
            Asset::FAssetDerivedKeyEvidence::CurrentKeyFormatVersion;
    if (IsMetalShaderSource(Node, Profile.Profile))
    {
        const auto Payload =
            std::dynamic_pointer_cast<const Asset::FShaderPayloadAsset>(
                Node.Payload);
        const Asset::FShaderAsset* Program = Payload
            ? FindShaderProgram(Plan, Payload->GetId()) : nullptr;
        if (!Payload || !Program || !MetalToolchain ||
            !MetalToolchain->IsValid())
            return Asset::EAssetResult::UnresolvedDependency;
        auto Parameters = Core::MakeShared<Private::FMetalShaderCookParameters>();
        Parameters->ShaderAssetId = Program->GetDesc().Id;
        Parameters->ShaderAssetVersion =
            VersionDigest(Program->GetDesc().Version);
        Parameters->InterfaceBindings = Program->GetDesc().InterfaceBindings;
        Parameters->Architecture = ArchitectureToken(
            Profile.Profile.CpuArchitecture);
        Parameters->ToolchainEvidence = *MetalToolchain;
        Parameters->WorkingDirectory = Core::FString(
            (std::filesystem::path(ScratchRoot.ToStdString()) /
             ("metal-shader-" + std::to_string(Node.PlanIndex))).generic_string());
        for (const auto& Source : Program->GetDesc().Stages)
            if (Source.Stage == Payload->GetStage())
                Parameters->GlslDigest = Source.ExpectedDigest;
        Private::FSpirvCrossMslResult Derivation;
        Core::TArray<Asset::FAssetDerivedNamedEvidence> Additional;
        const Asset::EAssetResult Prepared =
            Private::BuildMetalShaderDerivedEvidence(
            *Payload, *Parameters, Profile, Derivation, Additional);
        if (Prepared != Asset::EAssetResult::Success) return Prepared;
        Evidence.KeyFormatVersion =
            Asset::FAssetDerivedKeyEvidence::CurrentKeyFormatVersion;
        Evidence.AdditionalEvidence = std::move(Additional);
        Out.Parameters = std::move(Parameters);
    }
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
    auto MetalCooker = Core::MakeShared<Private::FMetalShaderCooker>();
    Asset::FAssetRegistrationToken MetalCookerToken;
    if (Registry.Register(MetalCooker, MetalCookerToken) !=
        Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::InternalFailure,
            "asset-cooker.metal-extension.failed", OutReport);
    Asset::FAssetRegistrationToken KTX2CookerToken;
    if (Asset::RegisterKTX2TextureCooker(Registry, KTX2CookerToken) !=
        Asset::EAssetResult::Success)
        return Fail(EAssetCookResultCategory::InternalFailure,
            "asset-cooker.ktx2-extension.failed", OutReport);
    Private::FMetalToolchainEvidence MetalToolchain;
    if (ParsedProfile.Profile.GraphicsBackend ==
        Asset::EAssetGraphicsBackend::Metal)
    {
        if (Private::InspectMetalToolchain(60000, 256U * 1024U,
                MetalToolchain) !=
            Private::EMetalLibraryFinalizeStatus::Success)
            return Fail(EAssetCookResultCategory::CookFailure,
                "asset-cooker.metal-toolchain.unavailable", OutReport);
    }
    const auto SharedProfile =
        Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(ParsedProfile);

    Core::TArray<FPreparedCookNode> Prepared(Plan.Nodes.size());
    for (const auto& Node : Plan.Nodes)
        if (PrepareCookNode(Plan, Node, ParsedProfile, Registry,
                ParsedProfile.Profile.GraphicsBackend ==
                        Asset::EAssetGraphicsBackend::Metal
                    ? &MetalToolchain : nullptr,
                Request.ScratchRoot,
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
    std::atomic<Core::uint32> FirstFailureIndex{
        std::numeric_limits<Core::uint32>::max()};
    Private::FAssetCookScheduleOutput Scheduled;
    const Asset::EAssetResult Schedule = Private::FAssetCookScheduler::Execute(
        Plan, Request.WorkerCount,
        [&Registry, &Catalog, &Snapshot, &ParsedProfile, &SharedProfile,
         &Request, &Prepared, &bCacheFailure, &FirstFailureIndex](
            const Private::FAssetCookGraphNode& Node,
            const Core::TArray<Private::FAssetCookScheduledResult>& Results)
        {
            (void)Results;
            auto& State = Prepared[Node.PlanIndex];
            const auto RecordFailure = [&](const char* Reason)
            {
                const std::string StableReason(Reason);
                State.StableReason = Core::FString(StableReason);
                Core::uint32 Current = FirstFailureIndex.load(
                    std::memory_order_relaxed);
                while (Node.PlanIndex < Current &&
                    !FirstFailureIndex.compare_exchange_weak(
                        Current, Node.PlanIndex,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
                {
                }
            };
            if (Private::FCookInputSnapshotBuilder::Revalidate(
                    Snapshot, ParsedProfile.Profile.Limits.MaxSourceBytes,
                    Catalog.Revalidate) != Asset::EAssetResult::Success)
            {
                RecordFailure("asset-cooker.source.changed");
                return Private::FAssetCookScheduledResult{
                    Asset::EAssetResult::TransientFailure, {}};
            }

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
                    RecordFailure(State.StableReason.CStr());
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
                        RecordFailure(State.StableReason.CStr());
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
                    RecordFailure(State.StableReason.CStr());
                    return Private::FAssetCookScheduledResult{
                        Asset::EAssetResult::ProcessingFailure, {}};
                }
            }
            Asset::FAssetCookRequest CookRequest;
            CookRequest.Metadata = Node.Metadata;
            CookRequest.Payload = Node.Payload;
            CookRequest.TargetProfile = ParsedProfile.Profile.DisplayName;
            CookRequest.TargetProfileEvidence = SharedProfile;
            CookRequest.Parameters = State.Parameters;
            Asset::FAssetCookResult Cooked = Asset::FAssetDispatch::Cook(
                Registry, State.CookerId, CookRequest);
            if (Cooked.Result != Asset::EAssetResult::Success)
            {
                RecordFailure("asset-cooker.cook.dispatch-failed");
                return Private::FAssetCookScheduledResult{
                    Cooked.Result, std::move(Cooked.Artifact)};
            }

            Asset::FAssetCookedPayloadLimits PayloadLimits;
            PayloadLimits.MaxEnvelopeBytes =
                ParsedProfile.Profile.Limits.MaxPayloadBytes;
            PayloadLimits.MaxBodyBytes =
                ParsedProfile.Profile.Limits.MaxPayloadBytes;
            Core::TArray<Core::uint8> NormalizedArtifact;
            const Asset::EAssetResult Normalized =
                Private::NormalizeCookedArtifact(
                    State.CookerId, State.OutputContract, Cooked, PayloadLimits,
                    NormalizedArtifact);
            if (Normalized != Asset::EAssetResult::Success)
            {
                RecordFailure("asset-cooker.cook.artifact-contract-failed");
                return Private::FAssetCookScheduledResult{Normalized, {}};
            }

            auto Installed = Private::FDerivedDataStore::Install(
                LookupRequest, NormalizedArtifact, Request.LeaseTimeout);
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
                            LookupRequest, NormalizedArtifact,
                            Request.LeaseTimeout);
                }
            }
            if (Installed.Result != Asset::EAssetResult::Success)
            {
                State.StableReason = Installed.StableReason;
                bCacheFailure.store(true, std::memory_order_relaxed);
                RecordFailure(State.StableReason.CStr());
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
    {
        const bool bCacheFailed = bCacheFailure.load(std::memory_order_relaxed);
        const char* Reason = bCacheFailed
            ? "asset-cooker.cache.failed"
            : Schedule == Asset::EAssetResult::TransientFailure
            ? "asset-cooker.source.changed"
            : "asset-cooker.cook.failed";
        const Core::uint32 FailureIndex = FirstFailureIndex.load(
            std::memory_order_relaxed);
        if (FailureIndex < Report.Assets.size())
        {
            auto& FailedAsset = Report.Assets[FailureIndex];
            FailedAsset.Decision = EAssetCookDecision::Failed;
            FailedAsset.Action = EAssetCookAction::Fail;
            FailedAsset.StableReason = Prepared[FailureIndex].StableReason;
            Report.Counts.Failed = 1;
        }
        OutReport = std::move(Report);
        FAssetCookResult Failed;
        Failed.Category = bCacheFailed
            ? EAssetCookResultCategory::CacheFailure : GraphCategory(Schedule);
        Failed.StableReason = Core::FString(Reason);
        return Failed;
    }

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
