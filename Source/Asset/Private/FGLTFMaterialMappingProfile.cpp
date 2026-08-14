#include "Asset/FGLTFMaterialMappingProfile.h"

#include <optional>

namespace Stoner::Asset
{

EAssetResult FGLTFMaterialMappingProfile::Validate() const
{
    const Core::FString* Names[] = {
        &Name, &BaseColorFactor, &MetallicFactor, &RoughnessFactor,
        &EmissiveFactor, &AlphaCutoff, &NormalScale, &OcclusionStrength,
        &BaseColorTexture, &MetallicRoughnessTexture, &NormalTexture,
        &OcclusionTexture, &EmissiveTexture};
    if (SchemaVersion != 1 || !SurfaceShader.IsValid() ||
        SurfaceShader.GetAssetType() != Core::FString("ShaderProgram"))
        return EAssetResult::InvalidInput;
    for (const Core::FString* Value : Names)
        if (Value->IsEmpty()) return EAssetResult::InvalidInput;
    return EAssetResult::Success;
}

FAssetDigest FGLTFMaterialMappingProfile::GetDigest() const
{
    Core::TArray<Core::uint8> Bytes;
    const Core::FString Values[] = {
        Name, SurfaceShader.ToString(), BaseColorFactor, MetallicFactor,
        RoughnessFactor, EmissiveFactor, AlphaCutoff, NormalScale,
        OcclusionStrength, BaseColorTexture, MetallicRoughnessTexture,
        NormalTexture, OcclusionTexture, EmissiveTexture};
    Bytes.push_back(static_cast<Core::uint8>(SchemaVersion));
    for (const Core::FString& Value : Values)
    {
        Bytes.insert(Bytes.end(), Value.View().begin(), Value.View().end());
        Bytes.push_back(0);
    }
    return FAssetDigest::FromBytes(Bytes);
}

EAssetResult MakeDefaultGLTFMaterialMappingProfile(
    FGLTFMaterialMappingProfile& OutProfile)
{
    OutProfile = {};
    const EAssetResult Result = FAssetId::Create(
        Core::FString("ShaderProgram"),
        Core::FString("Engine/Shaders/Deferred/Surface"),
        std::nullopt, OutProfile.SurfaceShader);
    return Result == EAssetResult::Success ? OutProfile.Validate() : Result;
}

} // namespace Stoner::Asset
