#include "FGLTFMaterialMapper.h"

#include "FMaterialDependencyExtractor.h"
#include "FMaterialShaderJsonCodec.h"

#include "cgltf/cgltf.h"

namespace Stoner::Asset::Private
{
namespace
{

EAssetResult MapSampler(
    const cgltf_sampler* Source,
    FMaterialSamplerIntent& Out)
{
    Out = {};
    if (Source == nullptr) return EAssetResult::Success;
    switch (Source->mag_filter)
    {
    case cgltf_filter_type_undefined:
    case cgltf_filter_type_linear: Out.MagFilter = EAssetSamplerFilter::Linear; break;
    case cgltf_filter_type_nearest: Out.MagFilter = EAssetSamplerFilter::Nearest; break;
    default: return EAssetResult::Unsupported;
    }
    switch (Source->min_filter)
    {
    case cgltf_filter_type_undefined:
        Out.MinFilter = EAssetSamplerFilter::Automatic;
        Out.MipFilter = EAssetSamplerMipFilter::Automatic;
        break;
    case cgltf_filter_type_nearest:
        Out.MinFilter = EAssetSamplerFilter::Nearest;
        Out.MipFilter = EAssetSamplerMipFilter::None;
        break;
    case cgltf_filter_type_linear:
        Out.MinFilter = EAssetSamplerFilter::Linear;
        Out.MipFilter = EAssetSamplerMipFilter::None;
        break;
    case cgltf_filter_type_nearest_mipmap_nearest:
        Out.MinFilter = EAssetSamplerFilter::Nearest;
        Out.MipFilter = EAssetSamplerMipFilter::Nearest;
        break;
    case cgltf_filter_type_linear_mipmap_nearest:
        Out.MinFilter = EAssetSamplerFilter::Linear;
        Out.MipFilter = EAssetSamplerMipFilter::Nearest;
        break;
    case cgltf_filter_type_nearest_mipmap_linear:
        Out.MinFilter = EAssetSamplerFilter::Nearest;
        Out.MipFilter = EAssetSamplerMipFilter::Linear;
        break;
    case cgltf_filter_type_linear_mipmap_linear:
        Out.MinFilter = EAssetSamplerFilter::Linear;
        Out.MipFilter = EAssetSamplerMipFilter::Linear;
        break;
    default: return EAssetResult::Unsupported;
    }
    const auto Address = [](cgltf_wrap_mode Value,
        EAssetSamplerAddressMode& Mapped)
    {
        switch (Value)
        {
        case cgltf_wrap_mode_repeat: Mapped = EAssetSamplerAddressMode::Repeat; return true;
        case cgltf_wrap_mode_mirrored_repeat:
            Mapped = EAssetSamplerAddressMode::MirroredRepeat; return true;
        case cgltf_wrap_mode_clamp_to_edge:
            Mapped = EAssetSamplerAddressMode::ClampToEdge; return true;
        default: return false;
        }
    };
    return Address(Source->wrap_s, Out.AddressU) &&
        Address(Source->wrap_t, Out.AddressV)
        ? EAssetResult::Success : EAssetResult::Unsupported;
}

EAssetResult AddBinding(
    const cgltf_data& Data,
    const cgltf_texture_view& View,
    ETextureSemantic Semantic,
    const Core::FString& Name,
    const Core::TArray<FGLTFTextureVariant>& Variants,
    Core::TArray<FMaterialAssetParameter>& Out)
{
    if (View.texture == nullptr) return EAssetResult::Success;
    if (View.has_transform || View.texcoord < 0 || View.texcoord > 1)
        return EAssetResult::Unsupported;
    const FAssetId* TextureId = FindGLTFTextureVariant(
        Variants, Data, View.texture, Semantic);
    if (TextureId == nullptr) return EAssetResult::DependencyMismatch;
    FMaterialSamplerIntent Sampler;
    EAssetResult Result = MapSampler(View.texture->sampler, Sampler);
    if (Result != EAssetResult::Success) return Result;
    FMaterialTextureBinding Binding;
    Result = FMaterialTextureBinding::Create(
        *TextureId, static_cast<Core::uint32>(View.texcoord), Sampler, Binding);
    if (Result != EAssetResult::Success) return Result;
    Out.push_back({Name,
        FMaterialAssetParameterValue::FromTextureBinding(std::move(Binding))});
    return EAssetResult::Success;
}

void AddScalar(Core::TArray<FMaterialAssetParameter>& Out,
    const Core::FString& Name, float Value)
{
    Out.push_back({Name, FMaterialAssetParameterValue::FromScalar(Value)});
}

} // namespace

EAssetResult MapGLTFMaterial(
    const cgltf_data& Data,
    const cgltf_material* Material,
    const FAssetId& MaterialId,
    const Core::TArray<FGLTFTextureVariant>& TextureVariants,
    const FGLTFMaterialMappingProfile& Mapping,
    Core::TSharedPtr<const FMaterialAsset>& OutMaterial,
    FAssetDiagnosticList* Diagnostics)
{
    OutMaterial.reset();
    if (Mapping.Validate() != EAssetResult::Success || !MaterialId.IsValid())
        return EAssetResult::InvalidInput;
    FMaterialAssetDesc Desc;
    Desc.Id = MaterialId;
    Desc.SchemaVersion = 2;
    Desc.Domain = EMaterialAssetDomain::Surface;
    Desc.BlendMode = Material == nullptr || Material->alpha_mode == cgltf_alpha_mode_opaque
        ? EMaterialAssetBlendMode::Opaque
        : Material->alpha_mode == cgltf_alpha_mode_mask
            ? EMaterialAssetBlendMode::Masked
            : EMaterialAssetBlendMode::Translucent;
    Desc.RenderState.bDepthTest = true;
    Desc.RenderState.bDepthWrite = Desc.BlendMode != EMaterialAssetBlendMode::Translucent;
    Desc.RenderState.bTwoSided = Material != nullptr && Material->double_sided;
    if (TSoftAssetRef<FShaderAsset>::Create(Mapping.SurfaceShader, Desc.Shader) !=
        EAssetResult::Success) return EAssetResult::InvalidIdentity;

    const cgltf_float DefaultBase[4] = {1, 1, 1, 1};
    const cgltf_float DefaultEmissive[3] = {0, 0, 0};
    const cgltf_float* Base = Material
        ? Material->pbr_metallic_roughness.base_color_factor : DefaultBase;
    const cgltf_float* Emissive = Material ? Material->emissive_factor : DefaultEmissive;
    Desc.Parameters.push_back({Mapping.BaseColorFactor,
        FMaterialAssetParameterValue::FromColor(
            Core::FColor(Base[0], Base[1], Base[2], Base[3]))});
    AddScalar(Desc.Parameters, Mapping.MetallicFactor,
        Material ? Material->pbr_metallic_roughness.metallic_factor : 1.0f);
    AddScalar(Desc.Parameters, Mapping.RoughnessFactor,
        Material ? Material->pbr_metallic_roughness.roughness_factor : 1.0f);
    Desc.Parameters.push_back({Mapping.EmissiveFactor,
        FMaterialAssetParameterValue::FromColor(
            Core::FColor(Emissive[0], Emissive[1], Emissive[2], 1.0f))});
    if (Material != nullptr && Material->alpha_mode == cgltf_alpha_mode_mask)
        AddScalar(Desc.Parameters, Mapping.AlphaCutoff, Material->alpha_cutoff);
    if (Material != nullptr)
    {
        if (Material->has_pbr_specular_glossiness || Material->unlit ||
            Material->has_clearcoat || Material->has_transmission ||
            Material->has_volume || Material->has_sheen ||
            Material->has_iridescence || Material->has_anisotropy)
            return EAssetResult::Unsupported;
        EAssetResult Result = AddBinding(Data,
            Material->pbr_metallic_roughness.base_color_texture,
            ETextureSemantic::Color, Mapping.BaseColorTexture,
            TextureVariants, Desc.Parameters);
        if (Result == EAssetResult::Success) Result = AddBinding(Data,
            Material->pbr_metallic_roughness.metallic_roughness_texture,
            ETextureSemantic::Data, Mapping.MetallicRoughnessTexture,
            TextureVariants, Desc.Parameters);
        if (Result == EAssetResult::Success) Result = AddBinding(Data,
            Material->normal_texture, ETextureSemantic::Normal,
            Mapping.NormalTexture, TextureVariants, Desc.Parameters);
        if (Result == EAssetResult::Success) Result = AddBinding(Data,
            Material->occlusion_texture, ETextureSemantic::Data,
            Mapping.OcclusionTexture, TextureVariants, Desc.Parameters);
        if (Result == EAssetResult::Success) Result = AddBinding(Data,
            Material->emissive_texture, ETextureSemantic::Color,
            Mapping.EmissiveTexture, TextureVariants, Desc.Parameters);
        if (Result != EAssetResult::Success) return Result;
        if (Material->normal_texture.texture != nullptr)
            AddScalar(Desc.Parameters, Mapping.NormalScale, Material->normal_texture.scale);
        if (Material->occlusion_texture.texture != nullptr)
            AddScalar(Desc.Parameters, Mapping.OcclusionStrength,
                Material->occlusion_texture.scale);
    }
    EAssetResult Result = ExtractMaterialDependencies(Desc);
    if (Result != EAssetResult::Success) return Result;
    FMaterialShaderDefinition Definition;
    Definition.Kind = EMaterialShaderDefinitionKind::Material;
    Definition.Value = Desc;
    Result = WriteMaterialShaderDefinition(
        Definition, Desc.CanonicalDefinition, Diagnostics);
    if (Result != EAssetResult::Success) return Result;
    Desc = std::get<FMaterialAssetDesc>(std::move(Definition.Value));
    Result = ExtractMaterialDependencies(Desc);
    if (Result != EAssetResult::Success) return Result;
    FMaterialAsset Asset;
    Result = FMaterialAsset::CreateValidated(std::move(Desc), Asset, Diagnostics);
    if (Result == EAssetResult::Success)
        OutMaterial = Core::MakeShared<const FMaterialAsset>(std::move(Asset));
    return Result;
}

} // namespace Stoner::Asset::Private
