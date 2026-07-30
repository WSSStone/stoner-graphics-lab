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
    }
    return RHI::ERHIDescriptorType::UniformBuffer;
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
        if (!SelectedStage.Payload ||
            SelectedStage.Payload->GetFormat() !=
                Asset::EShaderPayloadFormat::SPIRV ||
            SelectedStage.Payload->GetStage() != SelectedStage.Stage ||
            SelectedStage.Payload->GetBytes().size() % sizeof(Core::uint32) !=
                0)
        {
            Diagnose(Diagnostics, "MAT-ASSET-SHADER-PAYLOAD");
            return EMaterialResult::ValidationFailed;
        }

        RHI::FRHIShaderModuleDesc Module;
        Module.Stage = Stage(SelectedStage.Stage);
        Module.EntryPoint = SelectedStage.Payload->GetEntryPoint();
        Module.PayloadIdentity =
            SelectedStage.Payload->GetId().ToString();
        Module.DebugName = Module.PayloadIdentity;
        Module.Bytecode.Words.resize(
            SelectedStage.Payload->GetBytes().size() /
            sizeof(Core::uint32));
        std::memcpy(
            Module.Bytecode.Words.data(),
            SelectedStage.Payload->GetBytes().data(),
            SelectedStage.Payload->GetBytes().size());
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
