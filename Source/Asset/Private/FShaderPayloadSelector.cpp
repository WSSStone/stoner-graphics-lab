#include "Asset/FShaderAsset.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset
{

EAssetResult SelectShaderProgram(
    const FShaderAsset& Program,
    const FShaderTargetRequest& Request,
    const IShaderPayloadLookup& Payloads,
    FSelectedShaderProgram& OutSelection,
    FAssetDiagnosticList*)
{
    OutSelection = {};
    if (Request.AcceptableProfiles.empty() ||
        std::set<Core::FString>(
            Request.AcceptableProfiles.begin(),
            Request.AcceptableProfiles.end()).size() !=
            Request.AcceptableProfiles.size())
    {
        return EAssetResult::InvalidInput;
    }
    FShaderPermutationKey Canonical = Request.Permutation;
    std::sort(Canonical.Flags.begin(), Canonical.Flags.end());
    const auto& Desc = Program.GetDesc();
    const auto Variant = std::find_if(
        Desc.Variants.begin(),
        Desc.Variants.end(),
        [&Canonical](const auto& Candidate)
        {
            return Candidate.Permutation == Canonical;
        });
    if (Variant == Desc.Variants.end())
    {
        return EAssetResult::TargetUnavailable;
    }
    Core::TArray<EShaderStage> Required;
    for (const FShaderSourceReference& Stage : Desc.Stages)
    {
        Required.push_back(Stage.Stage);
    }
    for (const Core::FString& Profile : Request.AcceptableProfiles)
    {
        Core::TArray<FSelectedShaderStage> Selected;
        bool bAmbiguous = false;
        for (const EShaderStage Stage : Required)
        {
            Core::TArray<const FShaderPayloadReference*> Matches;
            for (const FShaderPayloadReference& Reference : Variant->Payloads)
            {
                if (Reference.Backend == Request.Backend &&
                    Reference.Profile == Profile &&
                    Reference.Stage == Stage)
                {
                    Matches.push_back(&Reference);
                }
            }
            if (Matches.size() > 1)
            {
                bAmbiguous = true;
                break;
            }
            if (Matches.empty())
            {
                Selected.clear();
                break;
            }
            const auto& Id = Matches.front()->Payload.GetId();
            if (!Id)
            {
                return EAssetResult::DependencyMismatch;
            }
            auto Payload = Payloads.Find(*Id);
            if (!Payload ||
                Payload->GetBackend() != Request.Backend ||
                Payload->GetProfile() != Profile ||
                Payload->GetStage() != Stage ||
                Payload->GetPermutation() != Canonical)
            {
                return EAssetResult::DependencyMismatch;
            }
            Selected.push_back({Stage, std::move(Payload)});
        }
        if (bAmbiguous)
        {
            return EAssetResult::AmbiguousTarget;
        }
        if (Selected.size() == Required.size())
        {
            OutSelection.ShaderId = Desc.Id;
            OutSelection.ShaderVersion = Desc.Version;
            OutSelection.Backend = Request.Backend;
            OutSelection.SelectedProfile = Profile;
            OutSelection.Permutation = std::move(Canonical);
            OutSelection.Stages = std::move(Selected);
            OutSelection.InterfaceBindings = Desc.InterfaceBindings;
            OutSelection.ConstantRanges = Desc.ConstantRanges;
            OutSelection.RequiredParameters = Desc.RequiredParameters;
            OutSelection.SourceManifest.push_back(
                {Desc.Id, Desc.Version, EAssetSourceRole::Program});
            for (const FShaderSourceReference& Source : Desc.Stages)
            {
                const auto& SourceId = Source.Source.GetId();
                if (!SourceId)
                {
                    OutSelection = {};
                    return EAssetResult::DependencyMismatch;
                }
                FAssetVersion SourceVersion;
                SourceVersion.SourceDigest = Source.ExpectedDigest;
                SourceVersion.ContentDigest = Source.ExpectedDigest;
                OutSelection.SourceManifest.push_back({
                    *SourceId,
                    SourceVersion,
                    EAssetSourceRole::Source});
            }
            for (const auto& Stage : OutSelection.Stages)
            {
                OutSelection.SourceManifest.push_back({
                    Stage.Payload->GetId(),
                    Stage.Payload->GetVersion(),
                    EAssetSourceRole::Payload});
            }
            return NormalizeSourceManifest(OutSelection.SourceManifest);
        }
    }
    return EAssetResult::TargetUnavailable;
}

} // namespace Stoner::Asset
