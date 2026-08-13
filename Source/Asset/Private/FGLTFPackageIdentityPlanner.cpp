#include "FGLTFPackageIdentityPlanner.h"

#include "FGLTFStableKey.h"

#include "cgltf/cgltf.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Stoner::Asset::Private
{
namespace
{

EAssetResult MakeId(
    const char* Type,
    const Core::FString& Path,
    const Core::FString& Key,
    FAssetId& OutId)
{
    return FAssetId::Create(
        Core::FString(Type), Path,
        std::optional<Core::FString>(Key), OutId);
}

template <typename TValue, typename TExtras>
EAssetResult PlanValues(
    const cgltf_data& Data,
    const TValue* Values,
    cgltf_size Count,
    const char* Type,
    const char* FallbackType,
    const Core::FString& Path,
    TExtras GetExtras,
    Core::TArray<Core::FString>& OutKeys,
    Core::TArray<FAssetId>& OutIds)
{
    std::set<Core::FString> ExplicitKeys;
    OutKeys.reserve(Count);
    OutIds.reserve(Count);
    for (cgltf_size Index = 0; Index < Count; ++Index)
    {
        const cgltf_extras& Extras = GetExtras(Values[Index]);
        const char* ExtrasJson = Extras.data;
        std::vector<char> CopiedExtras;
        if (ExtrasJson == nullptr && Extras.end_offset > Extras.start_offset)
        {
            cgltf_size Size = 0;
            if (cgltf_copy_extras_json(&Data, &Extras, nullptr, &Size) !=
                    cgltf_result_success ||
                Size == 0)
            {
                return EAssetResult::MalformedSource;
            }
            CopiedExtras.resize(Size);
            if (cgltf_copy_extras_json(
                    &Data, &Extras, CopiedExtras.data(), &Size) !=
                cgltf_result_success)
            {
                return EAssetResult::MalformedSource;
            }
            ExtrasJson = CopiedExtras.data();
        }
        Core::FString Key;
        bool Explicit = false;
        const EAssetResult KeyResult = MakeGLTFStableKey(
            ExtrasJson,
            Core::FString("idx." + std::string(FallbackType) + "." +
                std::to_string(Index)),
            Key,
            Explicit);
        if (KeyResult != EAssetResult::Success ||
            (Explicit && !ExplicitKeys.insert(Key).second))
        {
            return KeyResult == EAssetResult::Success
                ? EAssetResult::Conflict : KeyResult;
        }
        FAssetId Id;
        const EAssetResult IdResult = MakeId(Type, Path, Key, Id);
        if (IdResult != EAssetResult::Success) return IdResult;
        OutKeys.push_back(std::move(Key));
        OutIds.push_back(std::move(Id));
    }
    return EAssetResult::Success;
}

} // namespace

EAssetResult PlanGLTFPackageIdentities(
    const cgltf_data& Data,
    const Core::FString& LogicalPath,
    const FStaticModelImportProfile& Profile,
    FGLTFPackageIdentityPlan& OutPlan)
{
    OutPlan = {};
    if (Profile.Validate() != EAssetResult::Success || LogicalPath.IsEmpty() ||
        Data.meshes_count > Profile.Limits.MaxMeshes ||
        Data.scenes_count > Profile.Limits.MaxScenes ||
        Data.materials_count > Profile.Limits.MaxMaterials)
    {
        return EAssetResult::InvalidInput;
    }
    OutPlan.LogicalPath = LogicalPath;
    EAssetResult Result = PlanValues(
        Data, Data.meshes, Data.meshes_count, "StaticMesh", "mesh", LogicalPath,
        [](const cgltf_mesh& Value) -> const cgltf_extras& { return Value.extras; },
        OutPlan.MeshKeys, OutPlan.MeshIds);
    if (Result != EAssetResult::Success) return Result;
    Result = PlanValues(
        Data, Data.scenes, Data.scenes_count, "StaticModel", "scene", LogicalPath,
        [](const cgltf_scene& Value) -> const cgltf_extras& { return Value.extras; },
        OutPlan.SceneKeys, OutPlan.ModelIds);
    if (Result != EAssetResult::Success) return Result;
    Result = PlanValues(
        Data, Data.materials, Data.materials_count, "Material", "material", LogicalPath,
        [](const cgltf_material& Value) -> const cgltf_extras& { return Value.extras; },
        OutPlan.MaterialKeys, OutPlan.MaterialIds);
    if (Result != EAssetResult::Success) return Result;
    Result = PlanValues(
        Data, Data.images, Data.images_count, "Image", "image", LogicalPath,
        [](const cgltf_image& Value) -> const cgltf_extras& { return Value.extras; },
        OutPlan.ImageKeys, OutPlan.ImageIds);
    if (Result != EAssetResult::Success) return Result;
    Result = PlanValues(
        Data, Data.textures, Data.textures_count, "Texture", "texture", LogicalPath,
        [](const cgltf_texture& Value) -> const cgltf_extras& { return Value.extras; },
        OutPlan.TextureKeys, OutPlan.TextureIds);
    if (Result != EAssetResult::Success) return Result;
    return MakeId(
        "Material", LogicalPath, Core::FString("idx.material.default"),
        OutPlan.DefaultMaterialId);
}

} // namespace Stoner::Asset::Private
