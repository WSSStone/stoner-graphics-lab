#include "AssetGLTFMaterialTests.h"

#include "StaticModelTestSupport.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(FAssetGLTFMaterialTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

const FMaterialAssetParameter* FindParameter(
    const FMaterialAsset& Material, const char* Name)
{
    const auto& Parameters = Material.GetDesc().Parameters;
    const auto Found = std::find_if(Parameters.begin(), Parameters.end(),
        [Name](const auto& Parameter) { return Parameter.Name == FString(Name); });
    return Found == Parameters.end() ? nullptr : &*Found;
}

bool NearlyEqual(float Left, float Right)
{
    return std::abs(Left - Right) <= 1.0e-6f;
}
}

FAssetGLTFMaterialTestResult RunAssetGLTFMaterialTests()
{
    FAssetGLTFMaterialTestResult Result;
    EAssetResult ImportResult = EAssetResult::ProcessingFailure;
    FStaticModelImportRequest StaticRequest;
    StaticRequest.AssetRequest = MakeRequest(
        "Tests/Fixtures/StaticModel/Valid/Materials/01-pbr-all-embedded.gltf");
    StaticRequest.Profile = MakeShared<FStaticModelImportProfile>();
    TArray<FAssetImportOutput> Outputs;
    FAssetDiagnosticList Diagnostics;
    ImportResult = ImportStaticModel(StaticRequest, Outputs, &Diagnostics);
    const auto Materials = FindPayloads<FMaterialAsset>(Outputs);
    const auto Meshes = FindPayloads<FStaticMeshAsset>(Outputs);
    const auto Textures = FindPayloads<FTextureAsset>(Outputs);
    if (ImportResult != EAssetResult::Success)
        std::cout << "[DETAIL] PBR import result=" << static_cast<int>(ImportResult)
                  << " outputs=" << Outputs.size() << '\n';
    for (const auto& Diagnostic : Diagnostics)
        std::cout << "[DETAIL] diagnostic=" << Diagnostic.Code.CStr()
                  << " field=" << Diagnostic.Field.CStr() << '\n';
    const auto Mapped = std::find_if(Materials.begin(), Materials.end(),
        [](const auto& Material)
        { return Material->GetDesc().Id.GetSubresource() ==
            std::optional<FString>(FString("key.hero-material")); });
    Record(Result,
        ImportResult == EAssetResult::Success && Materials.size() == 2 &&
            Meshes.size() == 1 && Textures.size() == 3 &&
            Mapped != Materials.end(),
        "core PBR package emits mapped and default materials plus semantic textures");
    if (Mapped != Materials.end())
    {
        const FMaterialAsset& Material = **Mapped;
        const auto* Base = FindParameter(Material, "BaseColorFactor");
        const auto* Metallic = FindParameter(Material, "MetallicFactor");
        const auto* Roughness = FindParameter(Material, "RoughnessFactor");
        const auto* Alpha = FindParameter(Material, "AlphaCutoff");
        const auto* Emissive = FindParameter(Material, "EmissiveFactor");
        const auto* NormalScale = FindParameter(Material, "NormalScale");
        const auto* OcclusionStrength =
            FindParameter(Material, "OcclusionStrength");
        const auto BaseValue = Base &&
                std::holds_alternative<FColor>(Base->Value.Value)
            ? std::get<FColor>(Base->Value.Value)
            : FColor{};
        const auto EmissiveValue = Emissive &&
                std::holds_alternative<FColor>(Emissive->Value.Value)
            ? std::get<FColor>(Emissive->Value.Value)
            : FColor{};
        Record(Result,
            Material.GetDesc().SchemaVersion == 2 &&
                Material.GetDesc().BlendMode == EMaterialAssetBlendMode::Masked &&
                Material.GetDesc().RenderState.bTwoSided && Base && Metallic &&
                Roughness && Alpha && Emissive && NormalScale &&
                OcclusionStrength && NearlyEqual(BaseValue.R, 0.2f) &&
                NearlyEqual(BaseValue.G, 0.4f) &&
                NearlyEqual(BaseValue.B, 0.6f) &&
                NearlyEqual(BaseValue.A, 0.8f) &&
                NearlyEqual(std::get<float>(Metallic->Value.Value), 0.3f) &&
                NearlyEqual(std::get<float>(Roughness->Value.Value), 0.7f) &&
                NearlyEqual(std::get<float>(Alpha->Value.Value), 0.4f) &&
                NearlyEqual(EmissiveValue.R, 0.1f) &&
                NearlyEqual(EmissiveValue.G, 0.2f) &&
                NearlyEqual(EmissiveValue.B, 0.3f) &&
                NearlyEqual(std::get<float>(NormalScale->Value.Value), 0.75f) &&
                NearlyEqual(
                    std::get<float>(OcclusionStrength->Value.Value), 0.6f),
            "PBR factors alpha cutoff and two-sided state map to Material v2");
        const auto* BindingParameter = FindParameter(Material, "BaseColorTexture");
        bool BindingValid = false;
        if (BindingParameter && std::holds_alternative<FMaterialTextureBinding>(
                BindingParameter->Value.Value))
        {
            const auto& Binding = std::get<FMaterialTextureBinding>(
                BindingParameter->Value.Value);
            BindingValid = Binding.TexCoordSet == 1 &&
                Binding.Sampler.MagFilter == EAssetSamplerFilter::Nearest &&
                Binding.Sampler.MipFilter == EAssetSamplerMipFilter::Linear &&
                Binding.Sampler.AddressU == EAssetSamplerAddressMode::MirroredRepeat &&
                Binding.Sampler.AddressV == EAssetSamplerAddressMode::ClampToEdge;
        }
        Record(Result, BindingValid,
            "texture binding preserves UV1 and backend-neutral sampler intent");
    }
    Record(Result,
        !Meshes.empty() && Meshes.front()->GetDesc().Primitives.front().MaterialSlotIndex == 0 &&
            Meshes.front()->GetDesc().MaterialSlots.size() == 2 &&
            Meshes.front()->GetDesc().Primitives.front().Vertices.TexCoords[1].size() == 3,
        "mesh primitive references mapped material and retains default slot");

    const auto Default = std::find_if(Materials.begin(), Materials.end(),
        [](const auto& Material)
        {
            return Material->GetDesc().Id.GetSubresource() ==
                std::optional<FString>(FString("idx.material.default"));
        });
    const auto* DefaultBase = Default == Materials.end()
        ? nullptr : FindParameter(**Default, "BaseColorFactor");
    const auto* DefaultMetallic = Default == Materials.end()
        ? nullptr : FindParameter(**Default, "MetallicFactor");
    const auto* DefaultRoughness = Default == Materials.end()
        ? nullptr : FindParameter(**Default, "RoughnessFactor");
    Record(Result,
        Default != Materials.end() && DefaultBase && DefaultMetallic &&
            DefaultRoughness &&
            std::get<FColor>(DefaultBase->Value.Value) == FColor(1, 1, 1, 1) &&
            NearlyEqual(std::get<float>(DefaultMetallic->Value.Value), 1.0f) &&
            NearlyEqual(std::get<float>(DefaultRoughness->Value.Value), 1.0f),
        "package-local default material preserves glTF core defaults");

    FGLTFMaterialMappingProfile Mapping;
    const EAssetResult MappingResult = MakeDefaultGLTFMaterialMappingProfile(Mapping);
    const FAssetDigest MappingDigest = Mapping.GetDigest();
    Mapping.SchemaVersion = 2;
    Record(Result,
        MappingResult == EAssetResult::Success && MappingDigest.IsAvailable() &&
            Mapping.Validate() == EAssetResult::InvalidInput,
        "material mapping profile is explicit versioned and digestible");

    EAssetResult MissingUvResult = EAssetResult::Success;
    const auto MissingUv = Import(MakeRequest(
        "Tests/Fixtures/StaticModel/Invalid/Materials/02-missing-uv1.gltf"),
        MissingUvResult);
    Record(Result,
        MissingUvResult == EAssetResult::MalformedSource && MissingUv.empty(),
        "material binding rejects a texture coordinate set absent from its primitive");

    bool Deterministic = true;
    const FString First = FStaticModelInspection::FormatPackage(Outputs);
    for (int Index = 0; Index < 20; ++Index)
    {
        TArray<FAssetImportOutput> Repeated;
        ImportResult = ImportStaticModel(StaticRequest, Repeated);
        Deterministic = Deterministic && ImportResult == EAssetResult::Success &&
            FStaticModelInspection::FormatPackage(Repeated) == First;
    }
    Record(Result, Deterministic,
        "twenty PBR imports preserve package identity and dependency order");
    return Result;
}
