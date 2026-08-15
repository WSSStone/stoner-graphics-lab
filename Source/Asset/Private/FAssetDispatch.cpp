#include "Asset/FAssetDispatch.h"
#include "Asset/FAssetRegistry.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

namespace Stoner::Asset
{
namespace
{

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetStage Stage,
    EAssetResult Result,
    const Core::FString& Participant,
    const char* Code)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = Stage;
    Diagnostic.Result = Result;
    Diagnostic.Severity = Result == EAssetResult::Success
        ? EAssetDiagnosticSeverity::Info
        : EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = Participant;
    Diagnostics->push_back(std::move(Diagnostic));
}

bool Contains(const Core::TArray<Core::FString>& Values, const Core::FString& Value)
{
    return std::find(Values.begin(), Values.end(), Value) != Values.end();
}

bool ValidateImportOutputs(const Core::TArray<FAssetImportOutput>& Outputs)
{
    if (Outputs.empty())
    {
        return false;
    }
    std::set<FAssetId> Ids;
    for (const FAssetImportOutput& Output : Outputs)
    {
        if (Output.Metadata.Validate() != EAssetResult::Success ||
            !Ids.insert(Output.Metadata.Id).second ||
            (Output.Payload &&
             Output.Payload->GetAssetType() != Output.Metadata.Id.GetAssetType()))
        {
            return false;
        }
    }
    FAssetRegistry ValidationRegistry;
    FAssetMutationBatch Batch;
    for (const FAssetImportOutput& Output : Outputs)
    {
        Batch.Register(Output.Metadata);
    }
    return ValidationRegistry.Apply(Batch) == EAssetResult::Success;
}

} // namespace

FAssetResolveResult FAssetDispatch::Resolve(
    const FAssetExtensionRegistry& Registry,
    const FAssetResolveRequest& Request,
    FAssetDiagnosticList* Diagnostics)
{
    auto Candidates = Registry.Snapshot(EAssetExtensionKind::Resolver);
    Candidates.erase(
        std::remove_if(
            Candidates.begin(),
            Candidates.end(),
            [&Request](const FAssetExtensionCapability& Capability)
            {
                return !Contains(Capability.Schemes, Request.Location.GetScheme()) ||
                    (Request.RuntimeContext && !Capability.bRuntimeCompatible);
            }),
        Candidates.end());
    if (Candidates.empty())
    {
        return {EAssetResult::NoMatchingResolver, {}, {}};
    }
    const int HighestPriority = std::max_element(
        Candidates.begin(),
        Candidates.end(),
        [](const auto& Left, const auto& Right) { return Left.Priority < Right.Priority; })->Priority;
    Core::TArray<FAssetExtensionCapability> Leaders;
    std::copy_if(
        Candidates.begin(),
        Candidates.end(),
        std::back_inserter(Leaders),
        [HighestPriority](const auto& Candidate) { return Candidate.Priority == HighestPriority; });
    if (Leaders.size() != 1)
    {
        AddDiagnostic(Diagnostics, EAssetStage::Resolve, EAssetResult::AmbiguousResolver, {}, "asset.resolve.ambiguous");
        return {EAssetResult::AmbiguousResolver, {}, {}};
    }
    const FAssetExecutionLease Lease =
        Registry.Acquire(EAssetExtensionKind::Resolver, Leaders[0].Participant);
    const auto Resolver = Lease.Get<IAssetResolver>();
    if (!Resolver)
    {
        return {EAssetResult::RegistrationInactive, {}, {}};
    }
    FAssetResolveResult Result = Resolver->Resolve(Request);
    AddDiagnostic(
        Diagnostics,
        EAssetStage::Resolve,
        Result.Result,
        Leaders[0].Participant.ToString(),
        "asset.resolve.result");
    return Result;
}

EAssetResult FAssetDispatch::Import(
    const FAssetExtensionRegistry& Registry,
    const FAssetSourceDescriptor& Descriptor,
    const FAssetSourceLease& Source,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    return Import(
        Registry,
        FAssetImportRequest{Descriptor, Source, {}, {}},
        OutOutputs,
        Diagnostics);
}

EAssetResult FAssetDispatch::Import(
    const FAssetExtensionRegistry& Registry,
    const FAssetImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    OutOutputs.clear();
    auto Candidates = Registry.Snapshot(EAssetExtensionKind::Importer);
    if (Request.RuntimeContext)
        Candidates.erase(
            std::remove_if(Candidates.begin(), Candidates.end(),
                [](const FAssetExtensionCapability& Capability)
                {
                    return !Capability.bRuntimeCompatible;
                }),
            Candidates.end());
    // Format hints are advisory. Every registered importer still receives a
    // bounded probe so misleading or absent extensions cannot override content.
    if (Candidates.size() > 64)
    {
        return EAssetResult::CapacityExceeded;
    }
    if (Candidates.empty())
    {
        return EAssetResult::NoMatchingImporter;
    }

    struct FProbed
    {
        FAssetExtensionCapability Capability;
        FAssetExecutionLease Lease;
        int Confidence;
    };
    Core::TArray<FProbed> Probed;
    for (const FAssetExtensionCapability& Capability : Candidates)
    {
        FAssetExecutionLease Lease =
            Registry.Acquire(EAssetExtensionKind::Importer, Capability.Participant);
        const auto Importer = Lease.Get<IAssetImporter>();
        if (!Importer)
        {
            continue;
        }
        Core::TArray<Core::uint8> Prefix;
        const EAssetResult ReadResult =
            Request.Source.ReadPrefix(Capability.ProbeByteLimit, Prefix);
        if (ReadResult != EAssetResult::Success)
        {
            return ReadResult;
        }
        FAssetProbeResult Probe =
            Importer->Probe(Request.Descriptor, Prefix);
        if (Probe.Result != EAssetResult::Success ||
            Probe.Confidence < 0 || Probe.Confidence > 100)
        {
            return Probe.Result == EAssetResult::Success
                ? EAssetResult::InvalidInput
                : Probe.Result;
        }
        Probed.push_back({Capability, std::move(Lease), Probe.Confidence});
    }
    const auto Best = std::max_element(
        Probed.begin(),
        Probed.end(),
        [](const FProbed& Left, const FProbed& Right)
        {
            return Left.Confidence < Right.Confidence;
        });
    if (Best == Probed.end() || Best->Confidence == 0)
    {
        return EAssetResult::NoMatchingImporter;
    }
    const int BestConfidence = Best->Confidence;
    if (std::count_if(
            Probed.begin(),
            Probed.end(),
            [BestConfidence](const FProbed& Value)
            {
                return Value.Confidence == BestConfidence;
            }) != 1)
    {
        return EAssetResult::AmbiguousImporter;
    }

    const auto Importer = Best->Lease.Get<IAssetImporter>();
    EAssetResult Result =
        Importer->Import(Request, OutOutputs, Diagnostics);
    if (Result == EAssetResult::Success && !ValidateImportOutputs(OutOutputs))
    {
        OutOutputs.clear();
        Result = EAssetResult::InvalidInput;
    }
    AddDiagnostic(
        Diagnostics,
        EAssetStage::Import,
        Result,
        Best->Capability.Participant.ToString(),
        "asset.import.result");
    return Result;
}

FAssetLoadResult FAssetDispatch::Load(
    const FAssetExtensionRegistry& Registry,
    const FAssetParticipantId& Participant,
    const FAssetLoadRequest& Request)
{
    const auto Loader =
        Registry.Acquire(EAssetExtensionKind::Loader, Participant).Get<IAssetLoader>();
    return Loader ? Loader->Load(Request)
                  : FAssetLoadResult{
                        EAssetResult::RegistrationInactive, {}, {}};
}

FAssetCookResult FAssetDispatch::Cook(
    const FAssetExtensionRegistry& Registry,
    const FAssetParticipantId& Participant,
    const FAssetCookRequest& Request)
{
    const auto Cooker =
        Registry.Acquire(EAssetExtensionKind::Cooker, Participant).Get<IAssetCooker>();
    return Cooker ? Cooker->Cook(Request)
                  : FAssetCookResult{
                        EAssetResult::RegistrationInactive,
                        {},
                        {},
                        {},
                        {},
                        {}};
}

} // namespace Stoner::Asset
