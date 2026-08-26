#include "FProductionContentSession.h"

#include <algorithm>
#include <chrono>
#include <set>
#include <thread>
#include <variant>

namespace Stoner::Demo
{
namespace
{

using namespace Stoner::Asset;

using FAnyAssetHandle = std::variant<
    TAssetHandle<FImageAsset>,
    TAssetHandle<FKTX2TextureArtifact>,
    TAssetHandle<FShaderSourceAsset>,
    TAssetHandle<FShaderPayloadAsset>,
    TAssetHandle<FShaderAsset>,
    TAssetHandle<FMaterialAsset>,
    TAssetHandle<FMaterialInstanceAsset>,
    TAssetHandle<FStaticMeshAsset>,
    TAssetHandle<FStaticModelAsset>>;

template <typename T>
EAssetResult RequestTyped(
    FAssetManager& Manager,
    const FAssetCookManifestRecord& Record,
    FAssetRequestHandle& OutRequest)
{
    return Manager.Request<T>(Record.AssetId, OutRequest);
}

EAssetResult RequestRecord(
    FAssetManager& Manager,
    const FAssetCookManifestRecord& Record,
    FAssetRequestHandle& OutRequest)
{
    if (Record.AssetType == Core::FString("Image"))
        return RequestTyped<FImageAsset>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("Texture"))
        return RequestTyped<FKTX2TextureArtifact>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("ShaderSource"))
        return RequestTyped<FShaderSourceAsset>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("ShaderPayload"))
        return RequestTyped<FShaderPayloadAsset>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("ShaderProgram"))
        return RequestTyped<FShaderAsset>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("Material"))
        return RequestTyped<FMaterialAsset>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("MaterialInstance"))
        return RequestTyped<FMaterialInstanceAsset>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("StaticMesh"))
        return RequestTyped<FStaticMeshAsset>(Manager, Record, OutRequest);
    if (Record.AssetType == Core::FString("StaticModel"))
        return RequestTyped<FStaticModelAsset>(Manager, Record, OutRequest);
    return EAssetResult::Unsupported;
}

template <typename T>
EAssetResult FinishTyped(
    FAssetManager& Manager,
    FAssetRequestHandle Request,
    const FProductionContentSessionConfig& Config,
    FAnyAssetHandle& OutHandle,
    FProductionContentSessionInspection& Inspection,
    FAssetRequestHandle& OutReleasedRequest)
{
    EAssetResult Result = EAssetResult::Success;
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(Config.RequestTimeoutMilliseconds);
    FAssetRequestSnapshot Snapshot;
    while (std::chrono::steady_clock::now() < Deadline)
    {
        if (Config.RuntimeContext && Config.RuntimeContext->ShouldStop())
        {
            if (Manager.Cancel(Request) == EAssetResult::Success)
                ++Inspection.CancelledRequestCount;
        }
        Result = Manager.Query(Request, Snapshot);
        if (Result != EAssetResult::Success) break;
        if (Snapshot.State == EAssetRequestState::Ready ||
            Snapshot.State == EAssetRequestState::Failed ||
            Snapshot.State == EAssetRequestState::Cancelled)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (Snapshot.State == EAssetRequestState::Ready)
    {
        TAssetHandle<T> Handle;
        Result = Manager.GetResult(Request, Handle);
        if (Result == EAssetResult::Success && Handle.IsValid())
            OutHandle = std::move(Handle);
        else if (Result == EAssetResult::Success)
            Result = EAssetResult::InvalidHandle;
    }
    else if (Snapshot.State == EAssetRequestState::Cancelled)
        Result = EAssetResult::Cancelled;
    else if (Snapshot.State == EAssetRequestState::Failed)
        Result = Snapshot.Result;
    else if (Result == EAssetResult::Success)
        Result = EAssetResult::DeadlineExceeded;
    if (Manager.ReleaseRequest(Request) == EAssetResult::Success)
    {
        ++Inspection.ReleasedRequestCount;
        OutReleasedRequest = Request;
    }
    return Result;
}

EAssetResult FinishRecord(
    FAssetManager& Manager,
    const FAssetCookManifestRecord& Record,
    FAssetRequestHandle Request,
    const FProductionContentSessionConfig& Config,
    FAnyAssetHandle& OutHandle,
    FProductionContentSessionInspection& Inspection,
    FAssetRequestHandle& OutReleasedRequest)
{
    if (Record.AssetType == Core::FString("Image"))
        return FinishTyped<FImageAsset>(Manager, Request, Config, OutHandle,
            Inspection, OutReleasedRequest);
    if (Record.AssetType == Core::FString("Texture"))
        return FinishTyped<FKTX2TextureArtifact>(
            Manager, Request, Config, OutHandle, Inspection,
            OutReleasedRequest);
    if (Record.AssetType == Core::FString("ShaderSource"))
        return FinishTyped<FShaderSourceAsset>(
            Manager, Request, Config, OutHandle, Inspection,
            OutReleasedRequest);
    if (Record.AssetType == Core::FString("ShaderPayload"))
        return FinishTyped<FShaderPayloadAsset>(
            Manager, Request, Config, OutHandle, Inspection,
            OutReleasedRequest);
    if (Record.AssetType == Core::FString("ShaderProgram"))
        return FinishTyped<FShaderAsset>(Manager, Request, Config, OutHandle,
            Inspection, OutReleasedRequest);
    if (Record.AssetType == Core::FString("Material"))
        return FinishTyped<FMaterialAsset>(Manager, Request, Config, OutHandle,
            Inspection, OutReleasedRequest);
    if (Record.AssetType == Core::FString("MaterialInstance"))
        return FinishTyped<FMaterialInstanceAsset>(
            Manager, Request, Config, OutHandle, Inspection,
            OutReleasedRequest);
    if (Record.AssetType == Core::FString("StaticMesh"))
        return FinishTyped<FStaticMeshAsset>(Manager, Request, Config, OutHandle,
            Inspection, OutReleasedRequest);
    if (Record.AssetType == Core::FString("StaticModel"))
        return FinishTyped<FStaticModelAsset>(Manager, Request, Config, OutHandle,
            Inspection, OutReleasedRequest);
    return EAssetResult::Unsupported;
}

template <typename T>
Core::TSharedPtr<const T> ShareHandle(const FAnyAssetHandle& Handle)
{
    const auto* Typed = std::get_if<TAssetHandle<T>>(&Handle);
    return Typed && Typed->IsValid()
        ? Typed->GetShared()
        : Core::TSharedPtr<const T>{};
}

template <typename T>
const FAssetVersion& IntrinsicVersion(const T& Payload)
{
    return Payload.GetDesc().Version;
}

template <>
const FAssetVersion& IntrinsicVersion(
    const FShaderPayloadAsset& Payload)
{
    return Payload.GetVersion();
}

FAssetVersion IntrinsicVersion(const FKTX2TextureArtifact& Payload)
{
    FAssetVersion Version;
    Version.SourceDigest = Payload.GetInfo().SourceDigest;
    Version.ContentDigest = Payload.GetInfo().ContentDigest;
    return Version;
}

template <typename T>
void AddDependency(
    const FAnyAssetHandle& Handle,
    Core::TArray<Core::TSharedPtr<const T>>& Out,
    Renderer::FStaticModelRealizationDependencies& Dependencies)
{
    const auto* Typed = std::get_if<TAssetHandle<T>>(&Handle);
    if (!Typed || !Typed->IsValid()) return;
    Out.push_back(Typed->GetShared());
    Dependencies.Versions.push_back({
        Typed->GetIdentity(), IntrinsicVersion(**Typed)});
}

} // namespace

bool ShouldLoadProductionRootClosureFirst(
    const Core::FString& WorkloadRevision,
    Core::uint32 LifecycleCycles) noexcept
{
    return LifecycleCycles == 1000 &&
        WorkloadRevision == Core::FString("production-content-sponza-v2");
}

bool ShouldReuseProductionCookedEnvelopeAuthentication(
    const Core::FString& WorkloadRevision,
    Core::uint32 LifecycleCycles) noexcept
{
    return LifecycleCycles == 1000 &&
        (WorkloadRevision == Core::FString("production-content-lantern-v2") ||
         WorkloadRevision == Core::FString("production-content-sponza-v2"));
}

Core::uint32 SelectProductionContentWorkerCount(
    const Core::FString& WorkloadRevision,
    Core::uint32 LifecycleCycles,
    Asset::EAssetGraphicsBackend GraphicsBackend,
    Asset::EAssetTargetCpuArchitecture CpuArchitecture) noexcept
{
    if (LifecycleCycles == 1000)
    {
        return WorkloadRevision ==
            Core::FString("production-content-sponza-v2") ? 16u : 8u;
    }
    return GraphicsBackend == Asset::EAssetGraphicsBackend::Metal &&
        CpuArchitecture == Asset::EAssetTargetCpuArchitecture::Arm64
        ? 1u : 4u;
}

struct FProductionContentSession::FImpl
{
    Core::TSharedPtr<FAssetExtensionRegistry> Registry;
    FAssetRegistrationToken KTX2Loader;
    FAssetCookedExtensionRegistrations CookedLoaders;
    Core::TSharedPtr<FAssetManager> Manager;
    Core::TSharedPtr<FAssetCookedEnvelopeAuthentication>
        CookedEnvelopeAuthentication;
    Core::TSharedPtr<const FPublishedGenerationValidationResult>
        CookedGenerationValidation;
    Core::TArray<FAnyAssetHandle> Handles;
    FAssetRequestHandle LastReleasedRequest;
    FProductionContentSessionInspection Inspection;
};

bool FProductionContentSessionConfig::IsValid() const noexcept
{
    return !PublicationRoot.IsEmpty() && !LeaseCoordinationRoot.IsEmpty() &&
        !RootAssetIdentity.IsEmpty() && ExpectedGeneration.IsAvailable() &&
        TargetEvidence &&
        TargetEvidence->Validate() == EAssetResult::Success &&
        WorkerCount > 0 && WorkerCount <= 32 &&
        RequestTimeoutMilliseconds > 0 &&
        RequestTimeoutMilliseconds <= 600000;
}

FProductionContentSession::FProductionContentSession()
    : Impl_(Core::MakeUnique<FImpl>())
{
}

FProductionContentSession::~FProductionContentSession()
{
    (void)Shutdown();
    if (Impl_)
    {
        Impl_->CookedEnvelopeAuthentication.reset();
        Impl_->CookedGenerationValidation.reset();
    }
}

EAssetResult FProductionContentSession::Load(
    const FProductionContentSessionConfig& Config,
    FProductionContentLoadedClosure& OutClosure)
{
    OutClosure = {};
    if (!Impl_ || !Config.IsValid() || Impl_->Manager)
        return EAssetResult::InvalidInput;
    Impl_->Inspection = {};

    Core::TSharedPtr<const FPublishedGenerationValidationResult>
        ValidatedAuthority;
    if (Config.bReuseCookedEnvelopeAuthentication &&
        Impl_->CookedGenerationValidation)
    {
        ValidatedAuthority = Impl_->CookedGenerationValidation;
        Impl_->Inspection.bGenerationValidationReused = true;
    }
    else
    {
        FPublishedGenerationValidationRequest ValidationRequest;
        ValidationRequest.SubjectRoot = Config.PublicationRoot;
        ValidationRequest.ExpectedGenerationId = Config.ExpectedGeneration;
        // The strict manager validates every requested envelope digest,
        // header, and typed body below. Keep the first bind check scoped to the
        // immutable index/layout; the generation reader lease then makes this
        // payload-free authority safe to share with later manager lifetimes.
        ValidationRequest.Policy =
            EPublishedGenerationValidationPolicy::IndexAndLayout;
        auto Validated = FPublishedGenerationValidator::Validate(
            ValidationRequest);
        if (!Validated.Succeeded())
        {
            Impl_->Inspection.FirstFailure = Core::FString(
                "publication:" + Validated.StableReason.ToStdString());
            return Validated.Result;
        }
        ValidatedAuthority = Core::MakeShared<
            const FPublishedGenerationValidationResult>(
                std::move(Validated));
    }
    const auto& Validated = *ValidatedAuthority;
    if (Validated.Manifest.GenerationId != Config.ExpectedGeneration)
    {
        Impl_->Inspection.FirstFailure = "strict-generation-mismatch";
        return EAssetResult::Conflict;
    }
    if (Validated.Manifest.TargetProfile != *Config.TargetEvidence)
    {
        Impl_->Inspection.FirstFailure = "target-profile-mismatch";
        return EAssetResult::DependencyMismatch;
    }
    if (Config.bReuseCookedEnvelopeAuthentication)
    {
        if (!Impl_->CookedEnvelopeAuthentication)
        {
            const EAssetResult ContextResult =
                FAssetCookedEnvelopeAuthentication::Create(
                    Config.PublicationRoot,
                    Config.LeaseCoordinationRoot,
                    Validated.Manifest.GenerationId,
                    Config.RequestTimeoutMilliseconds,
                    static_cast<Core::uint32>(
                        Validated.Manifest.Records.size()),
                    Impl_->CookedEnvelopeAuthentication);
            if (ContextResult != EAssetResult::Success)
            {
                Impl_->Inspection.FirstFailure =
                    "envelope-authentication-create";
                return ContextResult;
            }
            Impl_->CookedGenerationValidation = ValidatedAuthority;
        }
        else if (!Impl_->CookedGenerationValidation)
        {
            Impl_->Inspection.FirstFailure =
                "envelope-authentication-authority-missing";
            return EAssetResult::Conflict;
        }
        const auto Authentication =
            Impl_->CookedEnvelopeAuthentication->Inspect();
        if (!Impl_->CookedEnvelopeAuthentication->MatchesBinding(
                Config.PublicationRoot,
                Validated.Manifest.GenerationId) ||
            Authentication.Capacity < Validated.Manifest.Records.size())
        {
            Impl_->Inspection.FirstFailure =
                "envelope-authentication-generation-mismatch";
            return EAssetResult::Conflict;
        }
    }
    const auto Root = std::find_if(
        Validated.Manifest.Selection.Roots.begin(),
        Validated.Manifest.Selection.Roots.end(),
        [&Config](const auto& Candidate)
        {
            return Candidate.ToString() == Config.RootAssetIdentity;
        });
    if (Root == Validated.Manifest.Selection.Roots.end() ||
        Root->GetAssetType() != Core::FString("StaticModel"))
    {
        Impl_->Inspection.FirstFailure = "strict-root-missing";
        return EAssetResult::NotFound;
    }

    Impl_->Registry = Core::MakeShared<FAssetExtensionRegistry>();
    if (RegisterCookedAssetExtensions(
            *Impl_->Registry, Impl_->CookedLoaders) != EAssetResult::Success ||
        RegisterKTX2TextureLoader(
            *Impl_->Registry, Impl_->KTX2Loader) != EAssetResult::Success)
    {
        Impl_->Inspection.FirstFailure = "loader-registration";
        (void)Shutdown();
        return EAssetResult::IncompleteRegistry;
    }
    FAssetManagerConfig ManagerConfig;
    ManagerConfig.Mode = EAssetManagerMode::StrictCooked;
    ManagerConfig.ExtensionRegistry = Impl_->Registry;
    ManagerConfig.PublicationRoot = Config.PublicationRoot;
    ManagerConfig.LeaseCoordinationRoot = Config.LeaseCoordinationRoot;
    ManagerConfig.TargetEvidence = Config.TargetEvidence;
    if (Config.bReuseCookedEnvelopeAuthentication)
    {
        ManagerConfig.CookedEnvelopeAuthentication =
            Impl_->CookedEnvelopeAuthentication;
        ManagerConfig.CookedGenerationValidation =
            Impl_->CookedGenerationValidation;
    }
    ManagerConfig.WorkerCount = Config.WorkerCount;
    FAssetDiagnosticList Diagnostics;
    EAssetResult Result = FAssetManager::Create(
        ManagerConfig, Impl_->Manager, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        Impl_->Inspection.FirstFailure = "manager-create";
        (void)Shutdown();
        return Result;
    }

    FProductionContentLoadedClosure Candidate;
    Candidate.GenerationIdentity = Validated.Manifest.GenerationId;
    Impl_->Inspection.GenerationIdentity = Validated.Manifest.GenerationId;
    Impl_->Inspection.ManifestAssetCount =
        static_cast<Core::uint32>(Validated.Manifest.Records.size());
    std::set<FAssetId> ModelClosure{*Root};
    Core::TArray<FAssetId> Pending{*Root};
    while (!Pending.empty())
    {
        const FAssetId Current = Pending.back();
        Pending.pop_back();
        const auto Record = std::lower_bound(
            Validated.Manifest.Records.begin(),
            Validated.Manifest.Records.end(), Current,
            [](const auto& CandidateRecord, const FAssetId& Id)
            { return CandidateRecord.AssetId < Id; });
        if (Record == Validated.Manifest.Records.end() ||
            Record->AssetId != Current)
        {
            Impl_->Inspection.FirstFailure = "model-closure-record-missing";
            (void)Shutdown();
            return EAssetResult::NotFound;
        }
        for (const auto& Dependency : Record->Dependencies)
            if (ModelClosure.insert(Dependency.AssetId).second)
                Pending.push_back(Dependency.AssetId);
    }
    struct FPendingRecord
    {
        const FAssetCookManifestRecord* Record = nullptr;
        FAssetRequestHandle Request;
    };
    struct FLoadedRecord
    {
        const FAssetCookManifestRecord* Record = nullptr;
        FAnyAssetHandle Handle;
    };
    Core::TArray<FLoadedRecord> LoadedRecords;
    LoadedRecords.reserve(Validated.Manifest.Records.size());

    if (Config.bLoadRootClosureFirst)
    {
        // The Sponza 1,000-cycle profile loads the selected model closure
        // first. Its external root handle retains every required dependency in
        // the manager cache, so later manifest-completeness requests become
        // cache hits instead of duplicate physical reads and typed decodes.
        const auto RootRecord = std::lower_bound(
            Validated.Manifest.Records.begin(),
            Validated.Manifest.Records.end(), *Root,
            [](const auto& CandidateRecord, const FAssetId& Id)
            { return CandidateRecord.AssetId < Id; });
        if (RootRecord == Validated.Manifest.Records.end() ||
            RootRecord->AssetId != *Root)
        {
            Impl_->Inspection.FirstFailure = "strict-root-record-missing";
            (void)Shutdown();
            return EAssetResult::NotFound;
        }
        FAssetRequestHandle RootRequest;
        Result = RequestRecord(*Impl_->Manager, *RootRecord, RootRequest);
        if (Result == EAssetResult::Success)
        {
            FAnyAssetHandle RootHandle;
            Result = FinishRecord(
                *Impl_->Manager, *RootRecord, RootRequest, Config, RootHandle,
                Impl_->Inspection, Impl_->LastReleasedRequest);
            if (Result == EAssetResult::Success)
                LoadedRecords.push_back(
                    {&*RootRecord, std::move(RootHandle)});
        }
        if (Result != EAssetResult::Success)
        {
            Impl_->Inspection.FirstFailure = Core::FString(
                "load:" + RootRecord->AssetId.ToString().ToStdString() +
                ";result=" + std::to_string(static_cast<int>(Result)));
            (void)Shutdown();
            return Result;
        }
    }

    Core::TArray<FPendingRecord> PendingRecords;
    PendingRecords.reserve(Validated.Manifest.Records.size());
    for (const auto& Record : Validated.Manifest.Records)
    {
        if (Config.bLoadRootClosureFirst && Record.AssetId == *Root) continue;
        FAssetRequestHandle Request;
        Result = RequestRecord(*Impl_->Manager, Record, Request);
        if (Result != EAssetResult::Success)
        {
            Impl_->Inspection.FirstFailure = Core::FString(
                "request:" + Record.AssetId.ToString().ToStdString() +
                ";result=" + std::to_string(static_cast<int>(Result)));
            (void)Shutdown();
            return Result;
        }
        PendingRecords.push_back({&Record, Request});
    }

    for (const FPendingRecord& PendingRecord : PendingRecords)
    {
        const auto& Record = *PendingRecord.Record;
        FAnyAssetHandle Handle;
        Result = FinishRecord(
            *Impl_->Manager, Record, PendingRecord.Request, Config,
            Handle, Impl_->Inspection,
            Impl_->LastReleasedRequest);
        if (Result != EAssetResult::Success)
        {
            Impl_->Inspection.FirstFailure = Core::FString(
                "load:" + Record.AssetId.ToString().ToStdString() +
                ";result=" + std::to_string(static_cast<int>(Result)));
            (void)Shutdown();
            return Result;
        }
        LoadedRecords.push_back({&Record, std::move(Handle)});
    }

    Impl_->Handles.reserve(LoadedRecords.size());
    for (FLoadedRecord& LoadedRecord : LoadedRecords)
    {
        const auto& Record = *LoadedRecord.Record;
        auto& Handle = LoadedRecord.Handle;
        const bool bModelDependency =
            Record.AssetId != *Root && ModelClosure.contains(Record.AssetId);
        if (Record.AssetId == *Root)
            Candidate.Model = ShareHandle<FStaticModelAsset>(Handle);
        else if (bModelDependency &&
                 Record.AssetType == Core::FString("StaticMesh"))
            AddDependency(Handle, Candidate.Dependencies.Meshes,
                Candidate.Dependencies);
        else if (bModelDependency &&
                 Record.AssetType == Core::FString("Material"))
            AddDependency(Handle, Candidate.Dependencies.Materials,
                Candidate.Dependencies);
        else if (bModelDependency &&
                 Record.AssetType == Core::FString("MaterialInstance"))
            AddDependency(Handle, Candidate.Dependencies.MaterialInstances,
                Candidate.Dependencies);
        else if (bModelDependency &&
                 Record.AssetType == Core::FString("ShaderProgram"))
            AddDependency(Handle, Candidate.Dependencies.Shaders,
                Candidate.Dependencies);
        else if (bModelDependency &&
                 Record.AssetType == Core::FString("ShaderPayload"))
            AddDependency(Handle, Candidate.Dependencies.ShaderPayloads,
                Candidate.Dependencies);
        else if (bModelDependency &&
                 Record.AssetType == Core::FString("Texture"))
            AddDependency(Handle, Candidate.Dependencies.Textures,
                Candidate.Dependencies);
        if (Record.AssetType == Core::FString("ShaderProgram"))
        {
            const auto Shader = ShareHandle<FShaderAsset>(Handle);
            if (Shader) Candidate.RenderShaders.push_back(Shader);
        }
        else if (Record.AssetType == Core::FString("ShaderPayload"))
        {
            const auto Payload = ShareHandle<FShaderPayloadAsset>(Handle);
            if (Payload) Candidate.RenderShaderPayloads.push_back(Payload);
        }
        Impl_->Handles.push_back(std::move(Handle));
    }
    if (!Candidate.Model)
    {
        Impl_->Inspection.FirstFailure = "strict-root-type";
        (void)Shutdown();
        return EAssetResult::TypeMismatch;
    }
    Candidate.LoadedAssetCount =
        static_cast<Core::uint32>(Impl_->Handles.size());
    Impl_->Inspection.LoadedAssetCount = Candidate.LoadedAssetCount;
    Impl_->Inspection.Manager = Impl_->Manager->Inspect();
    if (Impl_->CookedEnvelopeAuthentication)
        Impl_->Inspection.CookedEnvelopeAuthentication =
            Impl_->CookedEnvelopeAuthentication->Inspect();
    if (Impl_->Inspection.Manager.ResolverExecutions != 0 ||
        Impl_->Inspection.Manager.ImporterExecutions != 0 ||
        Impl_->Inspection.Manager.AuthoringDecoderExecutions != 0 ||
        Impl_->Inspection.Manager.SourceFallbackExecutions != 0)
    {
        Impl_->Inspection.FirstFailure = "strict-source-participant";
        (void)Shutdown();
        return EAssetResult::ProcessingFailure;
    }
    Impl_->Inspection.bPublished = true;
    OutClosure = std::move(Candidate);
    return EAssetResult::Success;
}

EAssetResult FProductionContentSession::Shutdown()
{
    if (!Impl_) return EAssetResult::Success;
    Impl_->Handles.clear();
    EAssetResult Result = EAssetResult::Success;
    if (Impl_->Manager)
    {
        Result = Impl_->Manager->Shutdown();
        Impl_->Inspection.Manager = Impl_->Manager->Inspect();
        if (Impl_->CookedEnvelopeAuthentication)
            Impl_->Inspection.CookedEnvelopeAuthentication =
                Impl_->CookedEnvelopeAuthentication->Inspect();
        FAssetRequestSnapshot StaleSnapshot;
        Impl_->Inspection.bStaleHandleRejected =
            Impl_->LastReleasedRequest.IsValid() &&
            Impl_->Manager->Query(
                Impl_->LastReleasedRequest, StaleSnapshot) ==
                EAssetResult::InvalidHandle;
        Impl_->Manager.reset();
    }
    Impl_->CookedLoaders.Reset();
    Impl_->KTX2Loader.Reset();
    Impl_->Registry.reset();
    Impl_->LastReleasedRequest = {};
    Impl_->Inspection.bShutdown = true;
    return Result;
}

const FProductionContentSessionInspection&
FProductionContentSession::Inspect() const noexcept
{
    return Impl_->Inspection;
}

} // namespace Stoner::Demo
