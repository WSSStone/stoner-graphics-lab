#include "RendererMaterialShaderTests.h"

#include "Renderer/RendererMinimal.h"

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

using namespace Stoner::Renderer;

void Record(FRendererMaterialShaderTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

FMaterialParameterValue Scalar(float Value)
{
    return FMaterialParameterValue::FromScalar(Value);
}

FMaterialParameterValue Vector(float X, float Y, float Z, float W)
{
    return FMaterialParameterValue::FromVector(Stoner::Core::FVector4(X, Y, Z, W));
}

FMaterialParameterValue Color(float R, float G, float B, float A)
{
    return FMaterialParameterValue::FromColor(Stoner::Core::FColor(R, G, B, A));
}

FMaterialParameterValue Texture(const char* Id)
{
    return FMaterialParameterValue::FromResourceReference(FMaterialResourceReference::Texture(Id));
}

FMaterialDesc MakeMaterialDesc(const char* Name, EMaterialDomain Domain, EMaterialBlendMode Blend, FShaderPermutation Permutation = {})
{
    FMaterialDesc Desc;
    Desc.Name = Name;
    Desc.ShaderReference = "SurfaceShader";
    Desc.Domain = Domain;
    Desc.BlendMode = Blend;
    Desc.PermutationRequest = std::move(Permutation);
    (void)Desc.Parameters.AddParameter("BaseColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
    (void)Desc.Parameters.AddParameter("Roughness", Scalar(0.5f));
    (void)Desc.Parameters.AddParameter("Tiling", Vector(1.0f, 1.0f, 0.0f, 0.0f));
    (void)Desc.Parameters.AddParameter("AlbedoTexture", Texture("Textures/DefaultWhite"));
    return Desc;
}

FShaderLibrary MakeShaderLibrary()
{
    FShaderLibrary Library;

    FShaderRecord Surface;
    Surface.ShaderId = "SurfaceShader";
    Surface.DiagnosticsName = "Representative Surface Shader";
    Surface.AllowedPermutationFlags = {"USE_MASK", "USE_TEXTURE", "USE_VERTEX_COLOR"};
    Surface.Variants = {
        {"SurfaceDefault", FShaderPermutation{}, "VS+PS"},
        {"SurfaceTextured", FShaderPermutation{{"USE_TEXTURE"}}, "VS+PS"},
        {"SurfaceMaskedTextured", FShaderPermutation{{"USE_MASK", "USE_TEXTURE"}}, "VS+PS"},
    };
    Surface.RequiredParameters = {
        {"AlbedoTexture", EMaterialParameterValueType::ResourceReference},
        {"BaseColor", EMaterialParameterValueType::Color},
        {"Roughness", EMaterialParameterValueType::Scalar},
        {"Tiling", EMaterialParameterValueType::Vector},
    };
    (void)Library.RegisterShaderRecord(std::move(Surface));

    FShaderRecord UI;
    UI.ShaderId = "UIShader";
    UI.DiagnosticsName = "UI Shader";
    UI.AllowedPermutationFlags = {"USE_TEXTURE"};
    UI.Variants = {{"UIDefault", FShaderPermutation{}, "VS+PS"}};
    UI.RequiredParameters = {{"BaseColor", EMaterialParameterValueType::Color}};
    (void)Library.RegisterShaderRecord(std::move(UI));

    FShaderRecord Post;
    Post.ShaderId = "PostShader";
    Post.DiagnosticsName = "Post Process Shader";
    Post.AllowedPermutationFlags = {"TONEMAP"};
    Post.Variants = {{"PostDefault", FShaderPermutation{}, "CS"}};
    (void)Library.RegisterShaderRecord(std::move(Post));

    return Library;
}

void TestMaterialDefinitions(FRendererMaterialShaderTestResult& Result)
{
    const struct
    {
        const char* Name;
        EMaterialDomain Domain;
        EMaterialBlendMode Blend;
    } Cases[] = {
        {"SurfaceOpaque", EMaterialDomain::Surface, EMaterialBlendMode::Opaque},
        {"SurfaceMasked", EMaterialDomain::Surface, EMaterialBlendMode::Masked},
        {"SurfaceTranslucent", EMaterialDomain::Surface, EMaterialBlendMode::Translucent},
        {"PostOpaque", EMaterialDomain::PostProcess, EMaterialBlendMode::Opaque},
        {"PostAdditive", EMaterialDomain::PostProcess, EMaterialBlendMode::Additive},
        {"UITranslucent", EMaterialDomain::UI, EMaterialBlendMode::Translucent},
        {"UIMasked", EMaterialDomain::UI, EMaterialBlendMode::Masked},
        {"DecalMasked", EMaterialDomain::Decal, EMaterialBlendMode::Masked},
    };

    int ValidCount = 0;
    for (const auto& Case : Cases)
    {
        FMaterial Material(MakeMaterialDesc(Case.Name, Case.Domain, Case.Blend));
        FMaterialDiagnosticLog Diagnostics;
        ValidCount += Material.Validate(&Diagnostics) == EMaterialResult::Success && !Diagnostics.HasErrors() ? 1 : 0;
    }
    Record(Result, ValidCount == 8, "Material definitions cover supported domains and blends");

    FMaterial Invalid(MakeMaterialDesc("InvalidPostMasked", EMaterialDomain::PostProcess, EMaterialBlendMode::Masked));
    FMaterialDiagnosticLog InvalidDiagnostics;
    Record(Result, Invalid.Validate(&InvalidDiagnostics) == EMaterialResult::UnsupportedCombination &&
            InvalidDiagnostics.Format().View().find("MAT-MATERIAL-DOMAIN-BLEND") != std::string_view::npos,
        "Material rejects unsupported domain and blend mode");

    FMaterialParameterSet Parameters;
    FMaterialDiagnosticLog DuplicateDiagnostics;
    const bool bDuplicateRejected = Parameters.AddParameter("Repeated", Scalar(1.0f), &DuplicateDiagnostics) == EMaterialResult::Success &&
        Parameters.AddParameter("Repeated", Scalar(2.0f), &DuplicateDiagnostics) == EMaterialResult::DuplicateName;
    Record(Result, bDuplicateRejected && DuplicateDiagnostics.Format().View().find("MAT-PARAM-DUPLICATE") != std::string_view::npos,
        "Material parameter set rejects duplicate names with stable diagnostics");

    FMaterial Stable(MakeMaterialDesc("StableDump", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    (void)Stable.Validate();
    const Stoner::Core::FString Dump = Stable.Dump();
    bool bStableDump = true;
    for (int Index = 0; Index < 20; ++Index)
    {
        bStableDump = bStableDump && Stable.Dump() == Dump;
    }
    Record(Result, bStableDump && Dump.View().find("Material StableDump") != std::string_view::npos, "Material inspection dump is byte-stable");
}

void TestMaterialInstances(FRendererMaterialShaderTestResult& Result)
{
    FMaterial Base(MakeMaterialDesc("BaseMaterial", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    (void)Base.Validate();

    FMaterialInstance NoOverride({ "NoOverride", &Base, nullptr, {} });
    FMaterialParameterSet NoOverrideParams;
    Record(Result, NoOverride.Validate() == EMaterialResult::Success &&
            NoOverride.ResolveEffectiveParameters(NoOverrideParams) == EMaterialResult::Success &&
            AreParameterValuesEqual(NoOverrideParams.FindParameter("Roughness")->Value, Base.GetParameters().FindParameter("Roughness")->Value),
        "Material instance inherits parent parameters without overrides");

    FMaterialParameterSet ParentOverrides;
    (void)ParentOverrides.AddParameter("Roughness", Scalar(0.25f));
    (void)ParentOverrides.AddParameter("AlbedoTexture", Texture("Textures/Parent"));
    FMaterialInstance Parent({ "ParentInstance", &Base, nullptr, ParentOverrides });

    FMaterialParameterSet ChildOverrides;
    (void)ChildOverrides.AddParameter("Roughness", Scalar(0.75f));
    (void)ChildOverrides.AddParameter("BaseColor", Color(0.2f, 0.4f, 0.6f, 1.0f));
    (void)ChildOverrides.AddParameter("Tiling", Vector(2.0f, 2.0f, 0.0f, 0.0f));
    FMaterialInstance Child({ "ChildInstance", nullptr, &Parent, ChildOverrides });
    FMaterialParameterSet Effective;
    const bool bResolved = Child.Validate() == EMaterialResult::Success &&
        Child.ResolveEffectiveParameters(Effective) == EMaterialResult::Success;
    Record(Result, bResolved &&
            Effective.FindParameter("Roughness")->Value.Scalar == 0.75f &&
            Effective.FindParameter("AlbedoTexture")->Value.ResourceReference.ReferenceId == "Textures/Parent" &&
            Effective.FindParameter("BaseColor")->Value.Color.B == 0.6f,
        "Material instance applies nearest override precedence across parameter types");

    FMaterialParameterSet UnknownOverrides;
    (void)UnknownOverrides.AddParameter("Missing", Scalar(1.0f));
    FMaterialInstance Unknown({ "UnknownOverride", &Base, nullptr, UnknownOverrides });
    FMaterialDiagnosticLog UnknownDiagnostics;
    Record(Result, Unknown.Validate(&UnknownDiagnostics) == EMaterialResult::NotFound &&
            UnknownDiagnostics.Format().View().find("MAT-INSTANCE-UNKNOWN-PARAM") != std::string_view::npos,
        "Material instance rejects overrides not defined by the root material");

    FMaterialParameterSet TypeOverrides;
    (void)TypeOverrides.AddParameter("Roughness", Color(1.0f, 0.0f, 0.0f, 1.0f));
    FMaterialInstance TypeMismatch({ "TypeMismatch", &Base, nullptr, TypeOverrides });
    Record(Result, TypeMismatch.Validate() == EMaterialResult::TypeMismatch, "Material instance rejects override type mismatch");

    FMaterialInstance CycleA({ "CycleA", &Base, nullptr, {} });
    FMaterialInstance CycleB({ "CycleB", nullptr, &CycleA, {} });
    CycleA.SetParentInstance(&CycleB);
    FMaterialDiagnosticLog CycleDiagnostics;
    Record(Result, CycleA.Validate(&CycleDiagnostics) == EMaterialResult::CycleDetected &&
            CycleDiagnostics.Format().View().find("MAT-INSTANCE-CYCLE") != std::string_view::npos,
        "Material instance detects inheritance cycles");

    Base.Invalidate();
    FMaterialInstance InvalidParent({ "InvalidParent", &Base, nullptr, {} });
    Record(Result, InvalidParent.Validate() == EMaterialResult::Invalidated, "Material instance rejects invalidated parent material");

    FMaterial InstanceBase(MakeMaterialDesc("InstanceBase", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    (void)InstanceBase.Validate();
    FMaterialInstance ParentToInvalidate({ "ParentToInvalidate", &InstanceBase, nullptr, {} });
    (void)ParentToInvalidate.Validate();
    FMaterialInstance ChildOfInvalidatedParent({ "ChildOfInvalidatedParent", nullptr, &ParentToInvalidate, {} });
    (void)ChildOfInvalidatedParent.Validate();
    ParentToInvalidate.Invalidate();
    FMaterialParameterSet InvalidatedParentInstanceParameters;
    FMaterialDiagnosticLog InvalidatedParentInstanceDiagnostics;
    Record(Result,
        ChildOfInvalidatedParent.ResolveEffectiveParameters(InvalidatedParentInstanceParameters, &InvalidatedParentInstanceDiagnostics) ==
            EMaterialResult::Invalidated &&
        InvalidatedParentInstanceDiagnostics.Format().View().find("MAT-INSTANCE-PARENT-INVALIDATED") != std::string_view::npos,
        "Material instance resolution rejects invalidated parent instances after validation");
}

void TestShaderLibraryAndBinding(FRendererMaterialShaderTestResult& Result)
{
    FShaderLibrary Library = MakeShaderLibrary();
    FMaterial Textured(MakeMaterialDesc("Textured", EMaterialDomain::Surface, EMaterialBlendMode::Opaque, FShaderPermutation{{"USE_TEXTURE"}}));
    (void)Textured.Validate();

    FMaterialShaderBinding Binding;
    Record(Result, ResolveMaterialShaderBinding(Textured, Library, Binding) == EMaterialResult::Success &&
            Binding.VariantId == "SurfaceTextured" && Binding.PermutationKey == "USE_TEXTURE",
        "Material shader binding selects registered shader variant");

    FShaderPermutation Reordered{{"USE_TEXTURE", "USE_MASK"}};
    FShaderPermutation ReorderedAgain{{"USE_MASK", "USE_TEXTURE"}};
    bool bPermutationStable = Reordered.GetCanonicalKey() == ReorderedAgain.GetCanonicalKey();
    for (int Index = 0; Index < 20; ++Index)
    {
        const FShaderVariant* Variant = nullptr;
        bPermutationStable = bPermutationStable &&
            Library.ResolveVariant("SurfaceShader", ReorderedAgain, Variant) == EMaterialResult::Success &&
            Variant != nullptr && Variant->VariantId == "SurfaceMaskedTextured";
    }
    Record(Result, bPermutationStable, "Shader permutation canonical key is stable across reordered flags and repeated resolutions");

    FShaderRecord DuplicateAllowedFlag;
    DuplicateAllowedFlag.ShaderId = "DuplicateAllowedFlag";
    DuplicateAllowedFlag.AllowedPermutationFlags = {"USE_TEXTURE", "USE_TEXTURE"};
    DuplicateAllowedFlag.Variants = {{"Default", FShaderPermutation{}, "VS+PS"}};
    FMaterialDiagnosticLog DuplicateAllowedFlagDiagnostics;
    Record(Result,
        Library.RegisterShaderRecord(DuplicateAllowedFlag, &DuplicateAllowedFlagDiagnostics) == EMaterialResult::DuplicateName &&
        DuplicateAllowedFlagDiagnostics.Format().View().find("MAT-SHADER-FLAG-DUPLICATE") != std::string_view::npos,
        "Shader library rejects duplicate allowed permutation flags at registration");

    FShaderRecord UndeclaredVariantFlag;
    UndeclaredVariantFlag.ShaderId = "UndeclaredVariantFlag";
    UndeclaredVariantFlag.AllowedPermutationFlags = {"USE_TEXTURE"};
    UndeclaredVariantFlag.Variants = {{"InvalidVariant", FShaderPermutation{{"NO_SUCH_FLAG"}}, "VS+PS"}};
    FMaterialDiagnosticLog UndeclaredVariantFlagDiagnostics;
    Record(Result,
        Library.RegisterShaderRecord(UndeclaredVariantFlag, &UndeclaredVariantFlagDiagnostics) == EMaterialResult::ValidationFailed &&
        UndeclaredVariantFlagDiagnostics.Format().View().find("MAT-SHADER-VARIANT-FLAG") != std::string_view::npos,
        "Shader library rejects variant permutation flags not declared by the record");

    FShaderRecord DuplicateVariantId;
    DuplicateVariantId.ShaderId = "DuplicateVariantId";
    DuplicateVariantId.AllowedPermutationFlags = {"USE_TEXTURE"};
    DuplicateVariantId.Variants = {
        {"Duplicated", FShaderPermutation{}, "VS+PS"},
        {"Duplicated", FShaderPermutation{{"USE_TEXTURE"}}, "VS+PS"},
    };
    FMaterialDiagnosticLog DuplicateVariantIdDiagnostics;
    Record(Result,
        Library.RegisterShaderRecord(DuplicateVariantId, &DuplicateVariantIdDiagnostics) == EMaterialResult::DuplicateName &&
        DuplicateVariantIdDiagnostics.Format().View().find("MAT-SHADER-VARIANT-DUPLICATE") != std::string_view::npos,
        "Shader library rejects duplicate shader variant ids at registration");

    FShaderRecord DuplicateVariantKey;
    DuplicateVariantKey.ShaderId = "DuplicateVariantKey";
    DuplicateVariantKey.AllowedPermutationFlags = {"USE_TEXTURE"};
    DuplicateVariantKey.Variants = {
        {"DefaultA", FShaderPermutation{}, "VS+PS"},
        {"DefaultB", FShaderPermutation{}, "VS+PS"},
    };
    FMaterialDiagnosticLog DuplicateVariantKeyDiagnostics;
    Record(Result,
        Library.RegisterShaderRecord(DuplicateVariantKey, &DuplicateVariantKeyDiagnostics) == EMaterialResult::DuplicateName &&
        DuplicateVariantKeyDiagnostics.Format().View().find("MAT-SHADER-VARIANT-KEY-DUPLICATE") != std::string_view::npos,
        "Shader library rejects duplicate shader variant permutation keys at registration");

    FMaterial UnknownFlag(MakeMaterialDesc("UnknownFlag", EMaterialDomain::Surface, EMaterialBlendMode::Opaque, FShaderPermutation{{"NO_SUCH_FLAG"}}));
    FMaterialShaderBinding UnknownFlagBinding;
    FMaterialDiagnosticLog UnknownFlagDiagnostics;
    Record(Result, ResolveMaterialShaderBinding(UnknownFlag, Library, UnknownFlagBinding, &UnknownFlagDiagnostics) == EMaterialResult::ValidationFailed &&
            UnknownFlagDiagnostics.Format().View().find("MAT-PERMUTATION-UNKNOWN-FLAG") != std::string_view::npos,
        "Shader library rejects unknown permutation flags before variant lookup");

    FMaterial MissingShader(MakeMaterialDesc("MissingShader", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    MissingShader.Reset(MakeMaterialDesc("MissingShader", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    FMaterialDesc MissingDesc = MakeMaterialDesc("MissingShader", EMaterialDomain::Surface, EMaterialBlendMode::Opaque);
    MissingDesc.ShaderReference = "NoSuchShader";
    MissingShader.Reset(MissingDesc);
    FMaterialShaderBinding MissingShaderBinding;
    Record(Result, ResolveMaterialShaderBinding(MissingShader, Library, MissingShaderBinding) == EMaterialResult::NotFound,
        "Shader library reports missing shader records");

    FMaterial MissingVariant(MakeMaterialDesc("MissingVariant", EMaterialDomain::Surface, EMaterialBlendMode::Opaque, FShaderPermutation{{"USE_VERTEX_COLOR"}}));
    FMaterialShaderBinding MissingVariantBinding;
    Record(Result, ResolveMaterialShaderBinding(MissingVariant, Library, MissingVariantBinding) == EMaterialResult::NotFound,
        "Shader library reports missing variants");

    FMaterialDesc MissingParamDesc = MakeMaterialDesc("MissingParam", EMaterialDomain::Surface, EMaterialBlendMode::Opaque);
    MissingParamDesc.Parameters.Clear();
    (void)MissingParamDesc.Parameters.AddParameter("BaseColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
    FMaterial MissingParam(MissingParamDesc);
    FMaterialShaderBinding MissingParamBinding;
    Record(Result, ResolveMaterialShaderBinding(MissingParam, Library, MissingParamBinding) == EMaterialResult::NotFound,
        "Material shader binding rejects missing required parameters");

    FMaterial InvalidatedParent(MakeMaterialDesc("InvalidatedParent", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    (void)InvalidatedParent.Validate();
    FMaterialInstance InstanceWithInvalidatedParent({ "InstanceWithInvalidatedParent", &InvalidatedParent, nullptr, {} });
    (void)InstanceWithInvalidatedParent.Validate();
    InvalidatedParent.Invalidate();
    FMaterialShaderBinding InvalidatedParentBinding;
    FMaterialDiagnosticLog InvalidatedParentDiagnostics;
    Record(Result,
        ResolveMaterialShaderBinding(InstanceWithInvalidatedParent, Library, InvalidatedParentBinding, &InvalidatedParentDiagnostics) ==
            EMaterialResult::Invalidated &&
        InvalidatedParentDiagnostics.Format().View().find("MAT-INSTANCE-PARENT-INVALIDATED") != std::string_view::npos,
        "Material shader binding rejects instances whose parent material is invalidated after validation");

    Library.InvalidateRecord("SurfaceShader");
    FMaterialShaderBinding InvalidatedBinding;
    Record(Result, ResolveMaterialShaderBinding(Textured, Library, InvalidatedBinding) == EMaterialResult::Invalidated,
        "Material shader binding rejects invalidated shader records");
}

void TestResourceRequirementsAndRenderGraphSmoke(FRendererMaterialShaderTestResult& Result)
{
    FMaterial Base(MakeMaterialDesc("ResourceBase", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    (void)Base.Validate();

    Stoner::Core::TArray<FMaterialResourceRequirement> BaseRequirements;
    Record(Result, ExtractMaterialResourceRequirements(Base, BaseRequirements) == EMaterialResult::Success &&
            BaseRequirements.size() == 1 &&
            BaseRequirements[0].Reference.ReferenceId == "Textures/DefaultWhite",
        "Material extracts abstract resource requirements from resource parameters");

    FMaterialParameterSet Overrides;
    (void)Overrides.AddParameter("AlbedoTexture", Texture("Textures/Override"));
    FMaterialInstance Instance({ "ResourceInstance", &Base, nullptr, Overrides });
    Stoner::Core::TArray<FMaterialResourceRequirement> InstanceRequirements;
    Record(Result, Instance.Validate() == EMaterialResult::Success &&
            ExtractMaterialResourceRequirements(Instance, InstanceRequirements) == EMaterialResult::Success &&
            InstanceRequirements.size() == 1 &&
            InstanceRequirements[0].Reference.ReferenceId == "Textures/Override",
        "Material instance resource requirements reflect resource-reference overrides");

    FMaterial InvalidatedResourceBase(MakeMaterialDesc("InvalidatedResourceBase", EMaterialDomain::Surface, EMaterialBlendMode::Opaque));
    (void)InvalidatedResourceBase.Validate();
    FMaterialInstance ResourceInstance({ "InvalidatedResourceInstance", &InvalidatedResourceBase, nullptr, Overrides });
    (void)ResourceInstance.Validate();
    InvalidatedResourceBase.Invalidate();
    Stoner::Core::TArray<FMaterialResourceRequirement> InvalidatedInstanceRequirements;
    FMaterialDiagnosticLog InvalidatedResourceDiagnostics;
    Record(Result,
        ExtractMaterialResourceRequirements(ResourceInstance, InvalidatedInstanceRequirements, &InvalidatedResourceDiagnostics) ==
            EMaterialResult::Invalidated &&
        InvalidatedResourceDiagnostics.Format().View().find("MAT-INSTANCE-PARENT-INVALIDATED") != std::string_view::npos,
        "Material instance resource requirements reject invalidated parents after validation");

    FMaterialDesc EmptyDesc = MakeMaterialDesc("NoResources", EMaterialDomain::Surface, EMaterialBlendMode::Opaque);
    EmptyDesc.Parameters.Clear();
    (void)EmptyDesc.Parameters.AddParameter("BaseColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
    FMaterial NoResources(EmptyDesc);
    Stoner::Core::TArray<FMaterialResourceRequirement> EmptyRequirements;
    Record(Result, ExtractMaterialResourceRequirements(NoResources, EmptyRequirements) == EMaterialResult::Success &&
            EmptyRequirements.empty(),
        "Material resource extraction succeeds with an empty requirement list");

    FMaterialParameterSet LiveParameters;
    FMaterialResourceReference LiveReference = FMaterialResourceReference::Texture("LiveResource");
    LiveReference.bLiveResource = true;
    (void)LiveParameters.AddParameter("LiveTexture", FMaterialParameterValue::FromResourceReference(LiveReference));
    Stoner::Core::TArray<FMaterialResourceRequirement> LiveRequirements;
    Record(Result, ExtractResourceRequirementsFromParameters(LiveParameters, "LiveMaterial", LiveRequirements) == EMaterialResult::ValidationFailed,
        "Material resource extraction rejects live resource references");

    FRenderGraph Graph("MaterialResourceSmoke");
    FRenderGraphBuilder Builder = Graph.CreateBuilder();
    const FRenderGraphResourceHandle TextureInput = Builder.ImportResource(FRenderGraphResourceDesc::Texture2D("Textures/Override", 16, 16));
    FRenderGraphPassDesc Pass = FRenderGraphPassDesc::Make("MaterialPass", ERenderGraphPassType::Graphics);
    Pass.Accesses.push_back({TextureInput, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    (void)Builder.AddPass(Pass);
    (void)Builder.MarkOutput(TextureInput);
    Record(Result, !InstanceRequirements.empty() && Graph.Compile() == ERenderGraphResult::Success,
        "Material resource requirements can be consumed by render graph declaration flow");
}

void TestRepresentativeElapsedAndDumps(FRendererMaterialShaderTestResult& Result)
{
    const auto Start = std::chrono::steady_clock::now();
    FShaderLibrary Library = MakeShaderLibrary();

    Stoner::Core::TArray<FMaterial> Materials;
    for (int Index = 0; Index < 5; ++Index)
    {
        FMaterialDesc Desc = MakeMaterialDesc((std::string("Representative") + std::to_string(Index)).c_str(),
            EMaterialDomain::Surface, EMaterialBlendMode::Opaque, FShaderPermutation{{"USE_TEXTURE"}});
        Materials.emplace_back(Desc);
        (void)Materials.back().Validate();
    }

    Stoner::Core::TArray<FMaterialInstance> Instances;
    for (int Index = 0; Index < 10; ++Index)
    {
        FMaterialParameterSet Overrides;
        (void)Overrides.AddParameter("Roughness", Scalar(0.1f * static_cast<float>(Index)));
        Instances.emplace_back(FMaterialInstanceDesc{Stoner::Core::FString(std::string("Instance") + std::to_string(Index)), &Materials[Index % Materials.size()], nullptr, Overrides});
        (void)Instances.back().Validate();
    }

    bool bStable = true;
    FMaterialShaderBinding Binding;
    Stoner::Core::TArray<FMaterialResourceRequirement> Requirements;
    const std::string Dump = Materials.front().Dump().ToStdString() + Library.Dump().ToStdString();
    for (int Index = 0; Index < 20; ++Index)
    {
        bStable = bStable &&
            ResolveMaterialShaderBinding(Instances[Index % Instances.size()], Library, Binding) == EMaterialResult::Success &&
            ExtractMaterialResourceRequirements(Instances[Index % Instances.size()], Requirements) == EMaterialResult::Success &&
            (Materials.front().Dump().ToStdString() + Library.Dump().ToStdString()) == Dump;
    }

    const auto End = std::chrono::steady_clock::now();
    const auto Elapsed = std::chrono::duration_cast<std::chrono::seconds>(End - Start).count();
    Record(Result, bStable && Elapsed < 60, "Representative material validation inspection and resource summary is under 60 seconds");
}

} // namespace

FRendererMaterialShaderTestResult RunRendererMaterialShaderTests()
{
    FRendererMaterialShaderTestResult Result;
    TestMaterialDefinitions(Result);
    TestMaterialInstances(Result);
    TestShaderLibraryAndBinding(Result);
    TestResourceRequirementsAndRenderGraphSmoke(Result);
    TestRepresentativeElapsedAndDumps(Result);
    return Result;
}
