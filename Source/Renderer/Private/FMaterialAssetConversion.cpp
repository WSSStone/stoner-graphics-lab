#include "Renderer/FMaterialAssetConversion.h"

#include <algorithm>
#include <variant>

namespace Stoner::Renderer
{
namespace
{

EMaterialDomain Domain(Asset::EMaterialAssetDomain Value)
{
    switch (Value)
    {
    case Asset::EMaterialAssetDomain::Surface: return EMaterialDomain::Surface;
    case Asset::EMaterialAssetDomain::PostProcess: return EMaterialDomain::PostProcess;
    case Asset::EMaterialAssetDomain::UI: return EMaterialDomain::UI;
    case Asset::EMaterialAssetDomain::Decal: return EMaterialDomain::Decal;
    }
    return EMaterialDomain::Surface;
}

EMaterialBlendMode Blend(Asset::EMaterialAssetBlendMode Value)
{
    switch (Value)
    {
    case Asset::EMaterialAssetBlendMode::Opaque: return EMaterialBlendMode::Opaque;
    case Asset::EMaterialAssetBlendMode::Translucent: return EMaterialBlendMode::Translucent;
    case Asset::EMaterialAssetBlendMode::Additive: return EMaterialBlendMode::Additive;
    case Asset::EMaterialAssetBlendMode::Masked: return EMaterialBlendMode::Masked;
    }
    return EMaterialBlendMode::Opaque;
}

EMaterialResult AddParameter(
    FMaterialParameterSet& Parameters,
    const Asset::FMaterialAssetParameter& Parameter,
    FMaterialDiagnosticLog* Diagnostics)
{
    switch (Parameter.Value.Type)
    {
    case Asset::EMaterialAssetParameterType::Scalar:
        return Parameters.AddParameter(
            Parameter.Name,
            FMaterialParameterValue::FromScalar(
                std::get<float>(Parameter.Value.Value)),
            Diagnostics);
    case Asset::EMaterialAssetParameterType::Vector:
        return Parameters.AddParameter(
            Parameter.Name,
            FMaterialParameterValue::FromVector(
                std::get<Core::FVector4>(Parameter.Value.Value)),
            Diagnostics);
    case Asset::EMaterialAssetParameterType::Color:
        return Parameters.AddParameter(
            Parameter.Name,
            FMaterialParameterValue::FromColor(
                std::get<Core::FColor>(Parameter.Value.Value)),
            Diagnostics);
    case Asset::EMaterialAssetParameterType::TextureReference:
        return Parameters.AddParameter(
            Parameter.Name,
            FMaterialParameterValue::FromResourceReference(
                FMaterialResourceReference::Texture(
                    std::get<Asset::FAssetId>(
                        Parameter.Value.Value).ToString())),
            Diagnostics);
    case Asset::EMaterialAssetParameterType::TextureBinding:
    {
        const auto& Binding = std::get<Asset::FMaterialTextureBinding>(
            Parameter.Value.Value);
        if (!Binding.Texture.GetId())
        {
            return EMaterialResult::ValidationFailed;
        }
        return Parameters.AddParameter(
            Parameter.Name,
            FMaterialParameterValue::FromResourceReference(
                FMaterialResourceReference::Texture(
                    Binding.Texture.GetId()->ToString())),
            Diagnostics);
    }
    }
    return EMaterialResult::ValidationFailed;
}

void Diagnose(FMaterialDiagnosticLog* Diagnostics, const char* Code)
{
    if (Diagnostics)
    {
        Diagnostics->Add(
            EMaterialDiagnosticSeverity::Error,
            EMaterialDiagnosticCategory::Material,
            EMaterialResult::ValidationFailed,
            Core::FString(Code),
            Core::FString("MaterialAsset"),
            Core::FString("material asset conversion failed"));
    }
}

} // namespace

EMaterialResult ConvertMaterialSamplerIntent(
    const Asset::FMaterialSamplerIntent& Intent,
    RHI::FRHISamplerDesc& OutSampler)
{
    if (!Asset::IsValidAssetSamplerFilter(Intent.MinFilter) ||
        !Asset::IsValidAssetSamplerFilter(Intent.MagFilter) ||
        !Asset::IsValidAssetSamplerMipFilter(Intent.MipFilter) ||
        !Asset::IsValidAssetSamplerAddressMode(Intent.AddressU) ||
        !Asset::IsValidAssetSamplerAddressMode(Intent.AddressV))
    {
        return EMaterialResult::ValidationFailed;
    }
    const auto Filter = [](Asset::EAssetSamplerFilter Value)
    {
        return Value == Asset::EAssetSamplerFilter::Nearest
            ? RHI::ERHISamplerFilter::Nearest
            : RHI::ERHISamplerFilter::Linear;
    };
    const auto Mip = [](Asset::EAssetSamplerMipFilter Value)
    {
        switch (Value)
        {
        case Asset::EAssetSamplerMipFilter::None:
            return RHI::ERHISamplerMipFilter::None;
        case Asset::EAssetSamplerMipFilter::Nearest:
            return RHI::ERHISamplerMipFilter::Nearest;
        case Asset::EAssetSamplerMipFilter::Linear:
        case Asset::EAssetSamplerMipFilter::Automatic:
            return RHI::ERHISamplerMipFilter::Linear;
        }
        return RHI::ERHISamplerMipFilter::Linear;
    };
    const auto Address = [](Asset::EAssetSamplerAddressMode Value)
    {
        switch (Value)
        {
        case Asset::EAssetSamplerAddressMode::Repeat:
            return RHI::ERHISamplerAddressMode::Repeat;
        case Asset::EAssetSamplerAddressMode::MirroredRepeat:
            return RHI::ERHISamplerAddressMode::MirroredRepeat;
        case Asset::EAssetSamplerAddressMode::ClampToEdge:
            return RHI::ERHISamplerAddressMode::ClampToEdge;
        }
        return RHI::ERHISamplerAddressMode::Repeat;
    };
    OutSampler = {};
    OutSampler.MinFilter = Filter(Intent.MinFilter);
    OutSampler.MagFilter = Filter(Intent.MagFilter);
    OutSampler.MipFilter = Mip(Intent.MipFilter);
    OutSampler.AddressU = Address(Intent.AddressU);
    OutSampler.AddressV = Address(Intent.AddressV);
    return RHI::IsValidRHISamplerDesc(OutSampler)
        ? EMaterialResult::Success
        : EMaterialResult::UnsupportedCombination;
}

EMaterialResult ConvertMaterialAsset(
    const FMaterialAssetConversionRequest& Request,
    FMaterialAssetSnapshot& OutSnapshot,
    FMaterialDiagnosticLog* Diagnostics)
{
    if (!Request.ResolvedMaterial ||
        !Request.Shader ||
        Request.Shader->ShaderRecords.size() != 1)
    {
        Diagnose(Diagnostics, "MAT-ASSET-MATERIAL-INPUT");
        return EMaterialResult::ValidationFailed;
    }
    const Asset::FResolvedMaterialAsset& Resolved =
        *Request.ResolvedMaterial;
    const FShaderRecord& ShaderRecord =
        Request.Shader->ShaderRecords.front();
    const auto ProgramVersion = std::find_if(
        Request.Shader->SourceManifest.begin(),
        Request.Shader->SourceManifest.end(),
        [](const Asset::FAssetSourceVersionRecord& Record)
        {
            return Record.Role == Asset::EAssetSourceRole::Program;
        });
    if (Resolved.Shader.IsEmpty() ||
        ProgramVersion == Request.Shader->SourceManifest.end() ||
        *Resolved.Shader.GetId() != ProgramVersion->Id ||
        ShaderRecord.ShaderId != Resolved.Shader.GetId()->ToString())
    {
        Diagnose(Diagnostics, "MAT-ASSET-MATERIAL-SHADER");
        return EMaterialResult::ValidationFailed;
    }

    FMaterialDesc Desc;
    Desc.Name = Resolved.LeafId.ToString();
    Desc.ShaderReference = ShaderRecord.ShaderId;
    Desc.Domain = Domain(Resolved.Domain);
    Desc.BlendMode = Blend(Resolved.BlendMode);
    Desc.RenderState = {
        Resolved.RenderState.bDepthTest,
        Resolved.RenderState.bDepthWrite,
        Resolved.RenderState.bTwoSided};
    Desc.PermutationRequest =
        FShaderPermutation(Resolved.PermutationRequest.Flags);
    Core::TArray<FMaterialAssetSnapshot::FTextureBinding> TextureBindings;
    for (const Asset::FMaterialAssetParameter& Parameter :
         Resolved.EffectiveParameters)
    {
        const EMaterialResult Result =
            AddParameter(Desc.Parameters, Parameter, Diagnostics);
        if (Result != EMaterialResult::Success)
        {
            return Result;
        }
        if (Parameter.Value.Type ==
                Asset::EMaterialAssetParameterType::TextureBinding)
        {
            const auto& Binding = std::get<Asset::FMaterialTextureBinding>(
                Parameter.Value.Value);
            FMaterialAssetSnapshot::FTextureBinding SnapshotBinding;
            SnapshotBinding.ParameterName = Parameter.Name;
            SnapshotBinding.TextureId = *Binding.Texture.GetId();
            SnapshotBinding.TexCoordSet = Binding.TexCoordSet;
            const EMaterialResult SamplerResult = ConvertMaterialSamplerIntent(
                Binding.Sampler, SnapshotBinding.Sampler);
            if (SamplerResult != EMaterialResult::Success)
            {
                return SamplerResult;
            }
            TextureBindings.push_back(std::move(SnapshotBinding));
        }
    }

    FShaderLibrary ValidationLibrary;
    EMaterialResult Result = ValidationLibrary.RegisterShaderRecords(
        Request.Shader->ShaderRecords,
        Diagnostics);
    if (Result != EMaterialResult::Success)
    {
        return Result;
    }
    const FShaderRecord* Registered =
        ValidationLibrary.FindRecord(ShaderRecord.ShaderId);
    if (!Registered ||
        (Result = ValidationLibrary.ValidateRequiredParameters(
             *Registered, Desc.Parameters, Diagnostics)) !=
            EMaterialResult::Success)
    {
        return Registered ? Result : EMaterialResult::NotFound;
    }

    FMaterialAssetSnapshot Candidate;
    Candidate.Material = FMaterial(std::move(Desc));
    Candidate.TextureBindings = std::move(TextureBindings);
    std::sort(
        Candidate.TextureBindings.begin(),
        Candidate.TextureBindings.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.ParameterName < Right.ParameterName;
        });
    Result = Candidate.Material.Validate(Diagnostics);
    if (Result != EMaterialResult::Success)
    {
        return Result;
    }
    Result = ExtractMaterialResourceRequirements(
        Candidate.Material,
        Candidate.ResourceRequirements,
        Diagnostics);
    if (Result != EMaterialResult::Success)
    {
        return Result;
    }
    Candidate.SourceManifest = Resolved.SourceManifest;
    Candidate.SourceManifest.insert(
        Candidate.SourceManifest.end(),
        Request.Shader->SourceManifest.begin(),
        Request.Shader->SourceManifest.end());
    if (Asset::NormalizeSourceManifest(Candidate.SourceManifest) !=
        Asset::EAssetResult::Success)
    {
        Diagnose(Diagnostics, "MAT-ASSET-MATERIAL-MANIFEST");
        return EMaterialResult::ValidationFailed;
    }
    OutSnapshot = std::move(Candidate);
    return EMaterialResult::Success;
}

} // namespace Stoner::Renderer
