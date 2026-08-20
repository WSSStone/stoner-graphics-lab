#include "Asset/FShaderAsset.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset
{
namespace
{

bool ReferenceCanProduceTarget(
    const FShaderPayloadReference& Reference,
    const FShaderTargetRequest& Request,
    const Core::FString& Profile)
{
    if (Reference.Backend == Request.Backend &&
        Reference.Profile == Profile)
        return true;
    return Request.Backend == EShaderBackendFamily::Metal &&
        Reference.Backend == EShaderBackendFamily::Vulkan &&
        Reference.Format == EShaderPayloadFormat::SPIRV;
}

bool IsDerivedMetalReference(
    const FShaderPayloadReference& Reference,
    const FShaderTargetRequest& Request)
{
    return Request.Backend == EShaderBackendFamily::Metal &&
        Reference.Backend == EShaderBackendFamily::Vulkan &&
        Reference.Format == EShaderPayloadFormat::SPIRV;
}

bool PayloadMatchesReference(
    const FShaderPayloadReference& Reference,
    const FShaderTargetRequest& Request,
    const Core::FString& Profile,
    EShaderStage Stage,
    const FShaderPermutationKey& Permutation,
    const Core::TArray<FShaderInterfaceBinding>& InterfaceBindings,
    const FShaderPayloadAsset& Payload)
{
    const bool bExact = Reference.Backend == Request.Backend;
    const bool bDerivedMetal = Request.Backend == EShaderBackendFamily::Metal &&
        Reference.Backend == EShaderBackendFamily::Vulkan &&
        Reference.Format == EShaderPayloadFormat::SPIRV;
    const Core::FString ExpectedArchitecture = Request.CpuArchitecture ==
            EAssetTargetCpuArchitecture::Arm64
        ? Core::FString("arm64") : Core::FString("x86_64");
    if ((!bExact && !bDerivedMetal) || Payload.GetBackend() != Request.Backend ||
        Payload.GetProfile() != Profile || Payload.GetStage() != Stage ||
        Payload.GetEntryPoint() != Reference.EntryPoint ||
        Payload.GetPermutation() != Permutation ||
        Payload.GetVersion().SourceDigest != Reference.ExpectedDigest)
        return false;
    if (bExact && Payload.GetFormat() != Reference.Format) return false;
    if (Request.Backend != EShaderBackendFamily::Metal) return bExact;
    const auto* BindingEvidence = Payload.GetNativeBindingEvidence();
    if (Payload.GetFormat() != EShaderPayloadFormat::MetalLibrary ||
        !BindingEvidence || !Payload.GetNativeLibraryEvidence() ||
        BindingEvidence->Validate() != EAssetResult::Success ||
        Payload.GetNativeLibraryEvidence()->Validate() != EAssetResult::Success ||
        Payload.GetNativeLibraryEvidence()->Architecture != ExpectedArchitecture)
        return false;
    Core::TArray<FShaderNativeBindingEntry> Expected;
    for (const auto& Binding : InterfaceBindings)
    {
        if (std::find(
                Binding.Visibility.begin(), Binding.Visibility.end(), Stage) ==
            Binding.Visibility.end())
            continue;
        for (Core::uint32 ArrayElement = 0;
             ArrayElement < Binding.ArrayCount;
             ++ArrayElement)
        {
            FShaderNativeBindingEntry Entry;
            Entry.Stage = Stage;
            Entry.SetIndex = Binding.SetIndex;
            Entry.BindingIndex = Binding.BindingIndex;
            Entry.DescriptorType = Binding.Kind;
            Entry.ArrayElement = ArrayElement;
            Expected.push_back(Entry);
        }
    }
    if (Expected.size() != BindingEvidence->Entries.size()) return false;
    for (Core::usize Index = 0; Index < Expected.size(); ++Index)
    {
        const auto& Actual = BindingEvidence->Entries[Index];
        if (Actual.Stage != Expected[Index].Stage ||
            Actual.SetIndex != Expected[Index].SetIndex ||
            Actual.BindingIndex != Expected[Index].BindingIndex ||
            Actual.DescriptorType != Expected[Index].DescriptorType ||
            Actual.ArrayElement != Expected[Index].ArrayElement)
            return false;
    }
    return true;
}

} // namespace

EAssetResult SelectShaderProgram(
    const FShaderAsset& Program,
    const FShaderTargetRequest& Request,
    const IShaderPayloadLookup& Payloads,
    FSelectedShaderProgram& OutSelection,
    FAssetDiagnosticList*)
{
    OutSelection = {};
    if (Request.AcceptableProfiles.empty() ||
        (Request.Backend == EShaderBackendFamily::Metal &&
         !Request.CpuArchitecture) ||
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
                if (ReferenceCanProduceTarget(Reference, Request, Profile) &&
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
            if (IsDerivedMetalReference(*Matches.front(), Request) &&
                (!Payload ||
                 Payload->GetBackend() != EShaderBackendFamily::Metal ||
                 Payload->GetFormat() != EShaderPayloadFormat::MetalLibrary ||
                 Payload->GetProfile() != Profile))
            {
                Selected.clear();
                break;
            }
            if (!Payload ||
                !PayloadMatchesReference(
                    *Matches.front(), Request, Profile, Stage,
                    Canonical, Desc.InterfaceBindings, *Payload))
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
