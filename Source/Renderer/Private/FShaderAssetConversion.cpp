#include "Renderer/FShaderAssetConversion.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <sstream>

namespace Stoner::Renderer
{
namespace
{

RHI::ERHIShaderStage Stage(Asset::EShaderStage Value)
{
    switch (Value)
    {
    case Asset::EShaderStage::Vertex: return RHI::ERHIShaderStage::Vertex;
    case Asset::EShaderStage::Fragment: return RHI::ERHIShaderStage::Fragment;
    case Asset::EShaderStage::Compute: return RHI::ERHIShaderStage::Compute;
    }
    return RHI::ERHIShaderStage::Unknown;
}

RHI::ERHIShaderStageFlags Visibility(
    const Core::TArray<Asset::EShaderStage>& Values)
{
    RHI::ERHIShaderStageFlags Result = RHI::ERHIShaderStageFlags::None;
    for (const Asset::EShaderStage Value : Values)
    {
        Result |= RHI::ToShaderStageFlag(Stage(Value));
    }
    return Result;
}

RHI::ERHIDescriptorType Descriptor(Asset::EShaderResourceKind Value)
{
    switch (Value)
    {
    case Asset::EShaderResourceKind::UniformBuffer:
        return RHI::ERHIDescriptorType::UniformBuffer;
    case Asset::EShaderResourceKind::SampledTexture:
        return RHI::ERHIDescriptorType::SampledTexture;
    case Asset::EShaderResourceKind::Sampler:
        return RHI::ERHIDescriptorType::Sampler;
    case Asset::EShaderResourceKind::StorageBuffer:
        return RHI::ERHIDescriptorType::StorageBuffer;
    case Asset::EShaderResourceKind::StorageTexture:
        return RHI::ERHIDescriptorType::StorageTexture;
    case Asset::EShaderResourceKind::CombinedTextureSampler:
        return RHI::ERHIDescriptorType::CombinedTextureSampler;
    }
    return RHI::ERHIDescriptorType::UniformBuffer;
}

RHI::ERHINativeResourceClass NativeClass(
    Asset::EShaderNativeResourceClass Value)
{
    switch (Value)
    {
    case Asset::EShaderNativeResourceClass::Buffer:
        return RHI::ERHINativeResourceClass::Buffer;
    case Asset::EShaderNativeResourceClass::Texture:
        return RHI::ERHINativeResourceClass::Texture;
    case Asset::EShaderNativeResourceClass::Sampler:
        return RHI::ERHINativeResourceClass::Sampler;
    }
    return static_cast<RHI::ERHINativeResourceClass>(255);
}

bool ConvertNativeBindingEvidence(
    const Asset::FShaderNativeBindingEvidence& Evidence,
    RHI::FRHINativeBindingMap& Out)
{
    Out = {};
    if (Evidence.Validate() != Asset::EAssetResult::Success) return false;
    Out.PolicyVersion = Evidence.PolicyVersion;
    for (const auto& Entry : Evidence.Entries)
    {
        Out.Entries.push_back({
            Stage(Entry.Stage), Entry.SetIndex, Entry.BindingIndex,
            Descriptor(Entry.DescriptorType), Entry.ArrayElement,
            NativeClass(Entry.NativeClass), Entry.NativeIndex});
    }
    for (const auto& Range : Evidence.ReservedRanges)
    {
        Out.ReservedRanges.push_back({
            Stage(Range.Stage), NativeClass(Range.NativeClass),
            Range.FirstIndex, Range.Count, Range.Purpose});
    }
    for (const auto& Limit : Evidence.LimitSnapshot)
    {
        Out.LimitSnapshot.push_back({
            Stage(Limit.Stage), NativeClass(Limit.NativeClass),
            Limit.MaxCount});
    }
    Out.CanonicalDigest.bAvailable = Evidence.CanonicalDigest.IsAvailable();
    Out.CanonicalDigest.Bytes = Evidence.CanonicalDigest.GetBytes();
    return RHI::IsCanonicalRHINativeBindingMap(Out);
}

EMaterialParameterValueType ParameterType(
    Asset::EMaterialAssetParameterType Value)
{
    switch (Value)
    {
    case Asset::EMaterialAssetParameterType::Scalar:
        return EMaterialParameterValueType::Scalar;
    case Asset::EMaterialAssetParameterType::Vector:
        return EMaterialParameterValueType::Vector;
    case Asset::EMaterialAssetParameterType::Color:
        return EMaterialParameterValueType::Color;
    case Asset::EMaterialAssetParameterType::TextureReference:
    case Asset::EMaterialAssetParameterType::TextureBinding:
        return EMaterialParameterValueType::ResourceReference;
    }
    return EMaterialParameterValueType::Scalar;
}

void Diagnose(FMaterialDiagnosticLog* Diagnostics, const char* Code)
{
    if (Diagnostics)
    {
        Diagnostics->Add(
            EMaterialDiagnosticSeverity::Error,
            EMaterialDiagnosticCategory::ShaderLibrary,
            EMaterialResult::ValidationFailed,
            Core::FString(Code),
            Core::FString("ShaderAsset"),
            Core::FString("shader asset conversion failed"));
    }
}

} // namespace

EMaterialResult ConvertShaderAsset(
    const FShaderAssetConversionRequest& Request,
    FShaderAssetSnapshot& OutSnapshot,
    FMaterialDiagnosticLog* Diagnostics)
{
    if (!Request.SelectedProgram ||
        Request.SelectedProgram->Stages.empty())
    {
        Diagnose(Diagnostics, "MAT-ASSET-SHADER-SELECTION");
        return EMaterialResult::ValidationFailed;
    }

    FShaderAssetSnapshot Candidate;
    const Asset::FSelectedShaderProgram& Selected =
        *Request.SelectedProgram;
    std::set<Asset::EShaderStage> StageSet;
    for (const Asset::FSelectedShaderStage& SelectedStage : Selected.Stages)
    {
        if (!StageSet.insert(SelectedStage.Stage).second)
        {
            Diagnose(Diagnostics, "MAT-ASSET-SHADER-STAGE-DUPLICATE");
            return EMaterialResult::ValidationFailed;
        }
    }
    const bool bGraphics =
        StageSet == std::set<Asset::EShaderStage>{
            Asset::EShaderStage::Vertex,
            Asset::EShaderStage::Fragment};
    const bool bCompute =
        StageSet == std::set<Asset::EShaderStage>{
            Asset::EShaderStage::Compute};
    if (!bGraphics && !bCompute)
    {
        Diagnose(Diagnostics, "MAT-ASSET-SHADER-STAGE-SET");
        return EMaterialResult::ValidationFailed;
    }
    Candidate.SourceManifest = Selected.SourceManifest;
    if (Asset::NormalizeSourceManifest(Candidate.SourceManifest) !=
        Asset::EAssetResult::Success)
    {
        Diagnose(Diagnostics, "MAT-ASSET-SHADER-MANIFEST");
        return EMaterialResult::ValidationFailed;
    }
    Candidate.SelectedTarget = {
        Selected.Backend,
        Selected.SelectedProfile,
        Selected.Permutation.ToString()};

    FShaderRecord Record;
    Record.ShaderId = Selected.ShaderId.ToString();
    Record.DiagnosticsName = Selected.ShaderId.ToString();
    FShaderVariant Variant;
    Variant.VariantId = Core::FString(
        Selected.SelectedProfile.ToStdString() + ":" +
        Selected.Permutation.ToString().ToStdString());
    Variant.Permutation =
        FShaderPermutation(Selected.Permutation.Flags);
    std::ostringstream StageSummary;

    for (std::size_t Index = 0;
         Index < Selected.Stages.size();
         ++Index)
    {
        const Asset::FSelectedShaderStage& SelectedStage =
            Selected.Stages[Index];
        const bool bSpirv = SelectedStage.Payload &&
            Selected.Backend == Asset::EShaderBackendFamily::Vulkan &&
            SelectedStage.Payload->GetFormat() ==
                Asset::EShaderPayloadFormat::SPIRV;
        const bool bMetalLibrary = SelectedStage.Payload &&
            Selected.Backend == Asset::EShaderBackendFamily::Metal &&
            SelectedStage.Payload->GetFormat() ==
                Asset::EShaderPayloadFormat::MetalLibrary;
        if ((!bSpirv && !bMetalLibrary) ||
            SelectedStage.Payload->GetStage() != SelectedStage.Stage ||
            (bSpirv &&
             SelectedStage.Payload->GetBytes().size() % sizeof(Core::uint32) !=
                 0))
        {
            Diagnose(Diagnostics, "MAT-ASSET-SHADER-PAYLOAD");
            return EMaterialResult::ValidationFailed;
        }

        RHI::FRHIShaderModuleDesc Module;
        Module.Stage = Stage(SelectedStage.Stage);
        Module.EntryPoint = SelectedStage.Payload->GetEntryPoint();
        Module.Payload.Format = bMetalLibrary
            ? RHI::ERHIShaderPayloadFormat::MetalLibrary
            : RHI::ERHIShaderPayloadFormat::SPIRV;
        Module.Payload.PayloadIdentity =
            SelectedStage.Payload->GetId().ToString();
        Module.Payload.TargetProfile = Selected.SelectedProfile;
        Module.Payload.Bytes = SelectedStage.Payload->GetBytes();
        Module.Payload.PayloadDigest =
            RHI::ComputeRHISha256(Module.Payload.Bytes);
        if (bMetalLibrary)
        {
            const auto* NativeEvidence =
                SelectedStage.Payload->GetNativeBindingEvidence();
            const auto* NativeLibrary =
                SelectedStage.Payload->GetNativeLibraryEvidence();
            if (!NativeEvidence || !NativeLibrary ||
                NativeLibrary->Validate() != Asset::EAssetResult::Success ||
                NativeLibrary->TargetProfile != Selected.SelectedProfile ||
                NativeLibrary->LibraryDigest !=
                    SelectedStage.Payload->GetVersion().ContentDigest ||
                NativeLibrary->LibraryDigest !=
                    Asset::FAssetDigest::FromBytes(
                        SelectedStage.Payload->GetBytes()) ||
                !ConvertNativeBindingEvidence(
                    *NativeEvidence, Module.NativeBindingMap))
            {
                Diagnose(Diagnostics, "MAT-ASSET-SHADER-NATIVE-BINDING");
                return EMaterialResult::ValidationFailed;
            }
            Module.ValidationMode =
                RHI::ERHIShaderBytecodeValidationMode::Runtime;
            Module.RuntimeMode = RHI::ERHIRuntimeObjectMode::RealRuntime;
        }
        Module.DebugName = Module.Payload.PayloadIdentity;
        Module.InterfaceMetadata.DebugName = Record.ShaderId;
        for (const Asset::FShaderInterfaceBinding& Binding :
             Selected.InterfaceBindings)
        {
            if (std::find(
                    Binding.Visibility.begin(),
                    Binding.Visibility.end(),
                    SelectedStage.Stage) == Binding.Visibility.end())
            {
                continue;
            }
            Module.InterfaceMetadata.Bindings.push_back({
                Binding.SetIndex,
                Binding.BindingIndex,
                Descriptor(Binding.Kind),
                Binding.ArrayCount,
                Visibility(Binding.Visibility)});
        }
        for (const Asset::FShaderConstantRange& Range :
             Selected.ConstantRanges)
        {
            if (std::find(
                    Range.Visibility.begin(),
                    Range.Visibility.end(),
                    SelectedStage.Stage) == Range.Visibility.end())
            {
                continue;
            }
            Module.InterfaceMetadata.ConstantRanges.push_back({
                Range.OffsetBytes,
                Range.SizeBytes,
                Visibility(Range.Visibility)});
        }
        if (!RHI::IsValidRHIShaderModuleDesc(Module))
        {
            Diagnose(Diagnostics, "MAT-ASSET-SHADER-RHI-DESC");
            return EMaterialResult::ValidationFailed;
        }
        if (Index) StageSummary << '+';
        StageSummary << static_cast<unsigned>(SelectedStage.Stage);
        Candidate.ModuleDescriptions.push_back(std::move(Module));
    }

    Variant.StageSummary = Core::FString(StageSummary.str());
    Record.Variants.push_back(std::move(Variant));
    Record.AllowedPermutationFlags = Selected.Permutation.Flags;
    for (const Asset::FShaderRequiredParameter& Required :
         Selected.RequiredParameters)
    {
        Record.RequiredParameters.push_back(
            {Required.Name, ParameterType(Required.Type)});
    }
    Candidate.ShaderRecords.push_back(std::move(Record));
    OutSnapshot = std::move(Candidate);
    return EMaterialResult::Success;
}

} // namespace Stoner::Renderer
