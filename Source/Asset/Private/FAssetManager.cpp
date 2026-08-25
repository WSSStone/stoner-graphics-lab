#include "Asset/FAssetManager.h"

#include "Asset/FAssetManagerInspection.h"
#include "Asset/FAssetCookedEnvelopeAuthentication.h"
#include "FAssetRequestTable.h"
#include "FAssetDependencyScheduler.h"
#include "FAssetLoadOperationTable.h"
#include "FAssetNodeLoadCoordinator.h"
#include "FAssetCompletionQueue.h"
#include "FAssetRuntimeCache.h"
#include "FAssetManagerInspection.h"
#include "FAssetWorkerExecutor.h"
#include "FBoundCookedGeneration.h"
#include "FCookedAssetLoadingStrategy.h"
#include "FDevelopmentAssetLoadingStrategy.h"
#include "IAssetLoadingStrategy.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <utility>
#include <vector>

namespace Stoner::Asset
{
namespace
{
std::atomic<Core::uint64> GManagerLifetime{1};
}

struct FAssetManager::FImpl
{
    FImpl(
        const FAssetManagerConfig& InConfig,
        Core::uint64 Lifetime,
        Core::TUniquePtr<Private::FBoundCookedGeneration> InGeneration,
        Core::TUniquePtr<Private::IAssetLoadingStrategy> InStrategy,
        Core::TSharedPtr<Private::FAssetManagerExecutionCounterState>
            InExecutionCounters)
        : Config(InConfig),
          Requests(Lifetime, InConfig.Limits.MaxRequests),
          Workers(InConfig.WorkerCount, InConfig.Limits.MaxQueuedWork),
          Cache(InConfig.Limits.MaxAggregatePayloadBytes),
          Completions(InConfig.Limits.MaxCompletions),
          BoundGeneration(std::move(InGeneration)),
          Strategy(std::move(InStrategy)),
          ExecutionCounters(std::move(InExecutionCounters))
    {
    }

    struct FRequestControl
    {
        FAssetRequestHandle Handle;
        Private::FAssetLoadKey Key;
        Core::TSharedPtr<Private::FSharedAssetLoadOperation> Operation;
        bool bCacheInterest = false;
        FAssetCompletionCallback Completion;
        bool bCompletionReserved = false;
        bool bCompletionQueued = false;
        bool bCacheHit = false;
        bool bCoalesced = false;
    };

    [[nodiscard]] FRequestControl* FindControl(
        FAssetRequestHandle Handle) const
    {
        const auto Found = std::find_if(
            RequestControls.begin(), RequestControls.end(),
            [Handle](const FRequestControl& Value)
            {
                return Value.Handle == Handle;
            });
        return Found == RequestControls.end()
            ? nullptr
            : const_cast<FRequestControl*>(&*Found);
    }

    void RemoveControl(FAssetRequestHandle Handle)
    {
        RequestControls.erase(
            std::remove_if(RequestControls.begin(), RequestControls.end(),
                [Handle](const FRequestControl& Value)
                {
                    return Value.Handle == Handle;
                }),
            RequestControls.end());
    }

    void CompleteRequest(FAssetRequestHandle Handle, EAssetResult Result)
    {
        std::lock_guard Lock(StateMutex);
        auto* Control = FindControl(Handle);
        if (!Control) return;
        Core::uint64 Sequence = NextCompletionSequence++;
        if (Sequence == 0) Sequence = NextCompletionSequence++;
        (void)Requests.SetCompletionSequence(Handle, Sequence);
        if (!Control->bCompletionReserved || !Control->Completion) return;
        if (Completions.Enqueue(
                Sequence, Handle, Result, Control->Completion))
        {
            Control->bCompletionQueued = true;
            Control->bCompletionReserved = false;
        }
        else
        {
            Completions.ReleaseReservation();
            Control->bCompletionReserved = false;
            ++CompletionContractViolations;
        }
    }

    FAssetManagerConfig Config;
    mutable std::mutex StateMutex;
    Private::FAssetRequestTable Requests;
    Private::FAssetWorkerExecutor Workers;
    Private::FAssetRuntimeCache Cache;
    Private::FAssetCompletionQueue Completions;
    Core::TUniquePtr<Private::FBoundCookedGeneration> BoundGeneration;
    Core::TUniquePtr<Private::IAssetLoadingStrategy> Strategy;
    Core::TSharedPtr<Private::FAssetManagerExecutionCounterState>
        ExecutionCounters;
    Private::FAssetLoadOperationTable Operations;
    Private::FAssetNodeLoadCoordinator NodeLoads;
    std::vector<FRequestControl> RequestControls;
    Core::uint64 ExtensionContractViolations = 0;
    Core::uint64 CompletionContractViolations = 0;
    Core::uint64 NextCompletionSequence = 1;
    bool ShuttingDown = false;
};

FAssetManager::FAssetManager(Core::TUniquePtr<FImpl> Impl)
    : Impl_(std::move(Impl))
{
}

FAssetManager::~FAssetManager()
{
    (void)Shutdown();
}

EAssetResult FAssetManager::Create(
    const FAssetManagerConfig& Config,
    Core::TSharedPtr<FAssetManager>& OutManager,
    FAssetDiagnosticList& OutDiagnostics)
{
    OutManager.reset();
    OutDiagnostics.clear();
    const EAssetResult Valid = Config.Validate();
    if (Valid != EAssetResult::Success) return Valid;

    Core::TUniquePtr<Private::FBoundCookedGeneration> Bound;
    if (Config.Mode == EAssetManagerMode::StrictCooked)
    {
        Bound = Core::MakeUnique<Private::FBoundCookedGeneration>();
        const EAssetResult Result = Private::FBoundCookedGeneration::Bind(
            Config, *Bound, OutDiagnostics);
        if (Result != EAssetResult::Success) return Result;
        if (Config.CookedEnvelopeAuthentication &&
            (!Config.CookedEnvelopeAuthentication->MatchesBinding(
                 Config.PublicationRoot,
                 Bound->GetPointer().GenerationId) ||
             Config.CookedEnvelopeAuthentication->Inspect().Capacity <
                 Bound->GetManifest().Records.size()))
            return EAssetResult::InvalidInput;
    }
    Core::TUniquePtr<Private::IAssetLoadingStrategy> Strategy;
    auto ExecutionCounters =
        Core::MakeShared<Private::FAssetManagerExecutionCounterState>();
    if (Config.Mode == EAssetManagerMode::DevelopmentSource)
        Strategy = Core::MakeUnique<Private::FDevelopmentAssetLoadingStrategy>(
            Config, ExecutionCounters);
    else
        Strategy = Core::MakeUnique<Private::FCookedAssetLoadingStrategy>(
            Config, *Bound, ExecutionCounters);
    Core::uint64 Lifetime = GManagerLifetime.fetch_add(
        1, std::memory_order_relaxed);
    if (Lifetime == 0)
        Lifetime = GManagerLifetime.fetch_add(1, std::memory_order_relaxed);
    OutManager = Core::TSharedPtr<FAssetManager>(new FAssetManager(
        Core::MakeUnique<FImpl>(
            Config, Lifetime, std::move(Bound), std::move(Strategy),
            std::move(ExecutionCounters))));
    return EAssetResult::Success;
}

EAssetResult FAssetManager::RequestUntyped(
    const FAssetId& Id,
    const Core::FString& ExpectedType,
    FAssetRequestHandle& OutRequest,
    FAssetCompletionCallback Completion)
{
    OutRequest = {};
    if (!Id.IsValid()) return EAssetResult::InvalidIdentity;
    if (Id.GetAssetType() != ExpectedType) return EAssetResult::TypeMismatch;
    {
        std::lock_guard Lock(Impl_->StateMutex);
        if (Impl_->ShuttingDown) return EAssetResult::ShuttingDown;
    }
    const bool bHasCompletion = static_cast<bool>(Completion);
    if (bHasCompletion && !Impl_->Completions.Reserve())
        return EAssetResult::CapacityExceeded;
    if (!Impl_->Requests.Allocate(OutRequest))
    {
        if (bHasCompletion) Impl_->Completions.ReleaseReservation();
        return EAssetResult::CapacityExceeded;
    }

    Private::FAssetLoadKey Key;
    Key.AssetId = Id;
    Key.ExpectedType = ExpectedType;
    Key.Mode = Impl_->Config.Mode;
    Key.TargetDigest =
        Impl_->Config.TargetEvidence->EffectiveProfileDigest;
    if (Impl_->BoundGeneration)
        Key.CookedGeneration =
            Impl_->BoundGeneration->GetPointer().GenerationId;

    Core::TSharedPtr<Private::FSharedAssetLoadOperation> Operation;
    Private::EAssetOperationAttachResult Attach =
        Private::EAssetOperationAttachResult::Created;
    bool bImmediateReady = false;
    bool bReadyCacheFailure = false;
    {
        std::lock_guard Lock(Impl_->StateMutex);
        if (Impl_->ShuttingDown)
        {
            (void)Impl_->Requests.Release(OutRequest);
            if (bHasCompletion) Impl_->Completions.ReleaseReservation();
            OutRequest = {};
            return EAssetResult::ShuttingDown;
        }
        Core::TSharedPtr<const FAssetPayload> CachedPayload;
        if (Impl_->Cache.AcquireRequest(Key, CachedPayload))
        {
            Impl_->RequestControls.push_back(
                {OutRequest, Key, {}, true, std::move(Completion),
                    bHasCompletion, false, true, false});
            (void)Impl_->Requests.CommitReady(OutRequest, CachedPayload);
            bImmediateReady = true;
        }
        else
        {
            Attach = Impl_->Operations.Attach(Key, OutRequest, Operation);
            Impl_->RequestControls.push_back(
                {OutRequest, Key, Operation, false, std::move(Completion),
                    bHasCompletion, false, false,
                    Attach == Private::EAssetOperationAttachResult::InFlight});
            if (Attach == Private::EAssetOperationAttachResult::ReadyCache)
            {
                Core::TSharedPtr<const FAssetPayload> ReadyPayload;
                if (Impl_->Cache.AcquireRequest(Key, ReadyPayload))
                {
                    if (auto* Control = Impl_->FindControl(OutRequest))
                        Control->bCacheInterest = true;
                    (void)Impl_->Requests.CommitReady(
                        OutRequest, std::move(ReadyPayload));
                    bImmediateReady = true;
                }
                else
                {
                    (void)Impl_->Operations.Detach(Operation, OutRequest);
                    if (bHasCompletion)
                        Impl_->Completions.ReleaseReservation();
                    Impl_->RemoveControl(OutRequest);
                    (void)Impl_->Requests.Release(OutRequest);
                    OutRequest = {};
                    bReadyCacheFailure = true;
                }
            }
        }
    }
    if (bReadyCacheFailure) return EAssetResult::ProcessingFailure;
    if (bImmediateReady)
    {
        Impl_->CompleteRequest(OutRequest, EAssetResult::Success);
        return EAssetResult::Success;
    }
    if (Attach == Private::EAssetOperationAttachResult::TerminalFailure)
    {
        (void)Impl_->Requests.CommitTerminal(OutRequest,
            Operation->State == Private::EAssetLoadOperationState::Cancelled
                ? EAssetRequestState::Cancelled
                : EAssetRequestState::Failed,
            Operation->Result);
        Impl_->CompleteRequest(OutRequest, Operation->Result);
        return EAssetResult::Success;
    }
    (void)Impl_->Requests.Transition(OutRequest,
        EAssetRequestState::Accepted,
        EAssetRequestState::WaitingForDependencies);
    if (Attach == Private::EAssetOperationAttachResult::InFlight)
        return EAssetResult::Success;

    FImpl* const State = Impl_.get();
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(
            Impl_->Config.ExtensionDeadlineMilliseconds);
    const auto Submit = Impl_->Workers.Submit(
        [State, Operation, Deadline]() mutable
        {
            {
                std::lock_guard Lock(State->StateMutex);
                for (const auto Interest : Operation->Interests)
                    (void)State->Requests.Transition(Interest,
                        EAssetRequestState::WaitingForDependencies,
                        EAssetRequestState::Loading);
            }
            const FAssetRuntimeExecutionContext Context{
                Operation->Cancellation, Deadline};
            Private::FAssetLoadScratchResult Loaded =
                Private::FAssetDependencyScheduler::LoadClosure(
                    Operation->Key, *State->Strategy, Context,
                    State->Config.Limits, &State->NodeLoads);
            if (std::chrono::steady_clock::now() >= Deadline &&
                Loaded.Result == EAssetResult::Cancelled)
                Loaded.Result = EAssetResult::DeadlineExceeded;
            Core::TSharedPtr<const FAssetPayload> Payload;
            EAssetResult Result = Loaded.Result;
            if (Result == EAssetResult::Success &&
                Loaded.Metadata.size() == Loaded.Payloads.size())
            {
                for (Core::usize Index = 0;
                     Index < Loaded.Metadata.size(); ++Index)
                {
                    if (Loaded.Metadata[Index].Id == Operation->Key.AssetId)
                    {
                        Payload = Loaded.Payloads[Index];
                        break;
                    }
                }
                if (!Payload)
                    Result = EAssetResult::NotFound;
                else if (Payload->GetAssetType() !=
                    Operation->Key.ExpectedType)
                    Result = EAssetResult::TypeMismatch;
            }
            else if (Result == EAssetResult::Success)
                Result = EAssetResult::ProcessingFailure;

            std::vector<FAssetRequestHandle> CompletedInterests;
            {
                std::lock_guard Lock(State->StateMutex);
                if (Operation->bTerminal) return;
                if (Loaded.bExtensionContractViolation)
                    ++State->ExtensionContractViolations;
                if (Result == EAssetResult::Success && Payload &&
                    !Operation->Interests.empty())
                {
                    Result = State->Cache.Publish(Operation->Key, Loaded,
                        static_cast<Core::uint64>(
                            Operation->Interests.size()), Payload);
                    if (Result == EAssetResult::Success)
                    {
                        for (const auto Interest : Operation->Interests)
                        {
                            if (auto* Control = State->FindControl(Interest))
                                Control->bCacheInterest = true;
                        }
                    }
                }
                Operation->Result = Result;
                Operation->FailurePath = Loaded.FailurePath;
                Operation->bTerminal = true;
                if (Result == EAssetResult::Success && Payload &&
                    !Operation->Interests.empty())
                {
                    Operation->Payload = Payload;
                    Operation->State =
                        Private::EAssetLoadOperationState::Ready;
                }
                else
                    Operation->State =
                        Result == EAssetResult::Cancelled
                        ? Private::EAssetLoadOperationState::Cancelled
                        : Private::EAssetLoadOperationState::Failed;
                for (const auto Interest : Operation->Interests)
                {
                    const bool Committed = Operation->State ==
                            Private::EAssetLoadOperationState::Ready
                        ? State->Requests.CommitReady(
                              Interest, Operation->Payload)
                        : State->Requests.CommitTerminal(Interest,
                              Operation->State == Private::
                                      EAssetLoadOperationState::Cancelled
                                  ? EAssetRequestState::Cancelled
                                  : EAssetRequestState::Failed,
                              Operation->Result);
                    if (Committed) CompletedInterests.push_back(Interest);
                }
                State->Operations.ReclaimIfUnretained(Operation);
            }
            for (const auto Interest : CompletedInterests)
                State->CompleteRequest(Interest,
                    Operation->State == Private::EAssetLoadOperationState::Ready
                        ? EAssetResult::Success
                        : Operation->Result);
        });
    if (Submit != Private::EAssetWorkerSubmitResult::Accepted)
    {
        {
            std::lock_guard Lock(Impl_->StateMutex);
            (void)Impl_->Operations.Detach(Operation, OutRequest);
            auto* Control = Impl_->FindControl(OutRequest);
            if (Control && Control->bCompletionReserved)
                Impl_->Completions.ReleaseReservation();
            Impl_->RemoveControl(OutRequest);
        }
        (void)Impl_->Requests.Release(OutRequest);
        OutRequest = {};
        return Submit == Private::EAssetWorkerSubmitResult::Stopped
            ? EAssetResult::ShuttingDown
            : EAssetResult::CapacityExceeded;
    }
    return EAssetResult::Success;
}

EAssetResult FAssetManager::Query(
    FAssetRequestHandle Request,
    FAssetRequestSnapshot& OutSnapshot) const
{
    return Impl_->Requests.Query(Request, OutSnapshot)
        ? EAssetResult::Success
        : EAssetResult::InvalidHandle;
}

EAssetResult FAssetManager::GetResultUntyped(
    FAssetRequestHandle Request,
    const Core::FString& ExpectedType,
    Core::TSharedPtr<Private::FAssetHandleControl>& OutControl) const
{
    OutControl.reset();
    FAssetRequestSnapshot Snapshot;
    if (!Impl_->Requests.Query(Request, Snapshot))
        return EAssetResult::InvalidHandle;
    if (Snapshot.State == EAssetRequestState::Cancelled)
        return EAssetResult::Cancelled;
    if (Snapshot.State == EAssetRequestState::Failed) return Snapshot.Result;
    if (Snapshot.State != EAssetRequestState::Ready)
        return EAssetResult::NotReady;
    Private::FAssetLoadKey Key;
    {
        std::lock_guard Lock(Impl_->StateMutex);
        const auto* Control = Impl_->FindControl(Request);
        if (!Control) return EAssetResult::InvalidHandle;
        Key = Control->Key;
    }
    if (Key.ExpectedType != ExpectedType)
        return EAssetResult::TypeMismatch;
    const EAssetResult Retained = Impl_->Cache.AcquireExternal(Key, OutControl);
    if (Retained != EAssetResult::Success || !OutControl)
        return Retained;
    if (OutControl->GetPayload()->GetAssetType() != ExpectedType)
    {
        OutControl.reset();
        return EAssetResult::TypeMismatch;
    }
    return EAssetResult::Success;
}

EAssetResult FAssetManager::Cancel(FAssetRequestHandle Request)
{
    bool Committed = false;
    {
        std::lock_guard Lock(Impl_->StateMutex);
        FAssetRequestSnapshot Snapshot;
        if (!Impl_->Requests.Query(Request, Snapshot))
            return EAssetResult::InvalidHandle;
        if (Snapshot.State == EAssetRequestState::Ready ||
            Snapshot.State == EAssetRequestState::Cancelled)
            return EAssetResult::Success;
        auto* Control = Impl_->FindControl(Request);
        if (Control && Control->Operation)
            (void)Impl_->Operations.Detach(Control->Operation, Request);
        Committed = Impl_->Requests.CommitTerminal(
            Request, EAssetRequestState::Cancelled, EAssetResult::Cancelled);
    }
    if (Committed)
    {
        Impl_->CompleteRequest(Request, EAssetResult::Cancelled);
        return EAssetResult::Success;
    }
    return EAssetResult::Conflict;
}

EAssetResult FAssetManager::ReleaseRequest(FAssetRequestHandle Request)
{
    FAssetRequestSnapshot Snapshot;
    if (!Impl_->Requests.Query(Request, Snapshot))
        return EAssetResult::InvalidHandle;
    {
        std::lock_guard Lock(Impl_->StateMutex);
        auto* Control = Impl_->FindControl(Request);
        if (Control)
        {
            if (Control->Operation)
                (void)Impl_->Operations.Detach(Control->Operation, Request);
            if (Control->bCacheInterest)
                Impl_->Cache.ReleaseRequest(Control->Key);
            if (Control->bCompletionQueued)
                (void)Impl_->Completions.Cancel(Request);
            else if (Control->bCompletionReserved)
                Impl_->Completions.ReleaseReservation();
        }
        Impl_->RemoveControl(Request);
    }
    if (!Impl_->Requests.Release(Request)) return EAssetResult::InvalidHandle;
    return EAssetResult::Success;
}

FAssetPumpResult FAssetManager::PumpCompletions(Core::uint32 MaxCount)
{
    return Impl_->Completions.Pump(MaxCount);
}

EAssetResult FAssetManager::Shutdown()
{
    {
        std::lock_guard Lock(Impl_->StateMutex);
        if (Impl_->ShuttingDown) return EAssetResult::Success;
        Impl_->ShuttingDown = true;
        for (const auto& Control : Impl_->RequestControls)
        {
            if (Control.Operation)
                Control.Operation->Cancellation->RequestCancellation();
        }
    }
    Impl_->Workers.RequestStop();
    Impl_->Workers.Join();
    {
        std::lock_guard Lock(Impl_->StateMutex);
        Impl_->Operations.Clear();
        Impl_->RequestControls.clear();
    }
    Impl_->Requests.DropPayloads();
    Impl_->Cache.ClearManagerOwnership();
    if (Impl_->BoundGeneration) Impl_->BoundGeneration->Reset();
    return EAssetResult::Success;
}

FAssetManagerInspection FAssetManager::Inspect() const
{
    FAssetManagerInspection Result;
    Result.Mode = Impl_->Config.Mode;
    Result.Limits = Impl_->Config.Limits;
    std::lock_guard Lock(Impl_->StateMutex);
    Result.bShuttingDown = Impl_->ShuttingDown;
    const auto Cache = Impl_->Cache.Inspect();
    Result.CachedAssets = Cache.Entries;
    Result.CachedPayloadBytes = Cache.PayloadBytes;
    Result.ExternalHandleRetentions = Cache.ExternalHandles;
    Result.RequestRetentions = Cache.RequestInterests;
    Result.RequiredDependencyRetentions = Cache.RequiredDependencies;
    Result.ExtensionContractViolations =
        Impl_->ExtensionContractViolations;
    Result.ResolverExecutions =
        Impl_->ExecutionCounters->ResolverExecutions.load(
            std::memory_order_relaxed);
    Result.ImporterExecutions =
        Impl_->ExecutionCounters->ImporterExecutions.load(
            std::memory_order_relaxed);
    Result.AuthoringDecoderExecutions =
        Impl_->ExecutionCounters->AuthoringDecoderExecutions.load(
            std::memory_order_relaxed);
    Result.SourceFallbackExecutions =
        Impl_->ExecutionCounters->SourceFallbackExecutions.load(
            std::memory_order_relaxed);
    Result.StrictLoaderExecutions =
        Impl_->ExecutionCounters->StrictLoaderExecutions.load(
            std::memory_order_relaxed);
    Result.CompletionReservations = Impl_->Completions.Reserved();
    Result.QueuedCompletions = Impl_->Completions.Queued();
    for (const auto& Control : Impl_->RequestControls)
    {
        FAssetRequestSnapshot Snapshot;
        if (!Impl_->Requests.Query(Control.Handle, Snapshot)) continue;
        Result.Requests.push_back({Snapshot, Control.Key.AssetId,
            Control.Key.ExpectedType, Control.bCacheHit,
            Control.bCoalesced});
        switch (Snapshot.State)
        {
        case EAssetRequestState::Ready: ++Result.ReadyRequests; break;
        case EAssetRequestState::Failed: ++Result.FailedRequests; break;
        case EAssetRequestState::Cancelled:
            ++Result.CancelledRequests;
            break;
        default: ++Result.AcceptedRequests; break;
        }
    }
    for (const auto& Operation : Impl_->Operations.Inspect())
    {
        Result.Operations.push_back({Operation.Key.AssetId,
            Operation.Key.ExpectedType, Operation.Key.Mode,
            Operation.Key.TargetDigest,
            Operation.Key.CookedGeneration.value_or(FAssetDigest{}),
            Operation.CallerInterests, Operation.Result,
            Operation.FailurePath});
    }
    Result.ActiveOperations =
        static_cast<Core::uint32>(Result.Operations.size());
    for (const auto& Entry : Impl_->Cache.InspectEntries())
        Result.Cache.push_back({Entry.Key.AssetId, Entry.Key.ExpectedType,
            Entry.PayloadBytes, Entry.Retentions.ExternalHandles,
            Entry.Retentions.RequestInterests,
            Entry.Retentions.RequiredDependencies});
    if (Impl_->BoundGeneration && Impl_->BoundGeneration->IsBound())
        Result.BoundGeneration =
            Impl_->BoundGeneration->GetPointer().GenerationId;
    Private::NormalizeAssetManagerInspection(
        Result, Impl_->Config.Limits.MaxDiagnostics);
    return Result;
}

} // namespace Stoner::Asset
