#include "RendererForwardPipelineTests.h"

#include "Renderer/RendererMinimal.h"

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

using namespace Stoner::Renderer;

void Record(FRendererForwardPipelineTestResult& Result, bool bPassed, const char* Name)
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

FForwardPBRSurfaceInputs CompletePBR()
{
    FForwardPBRSurfaceInputs Inputs;
    Inputs.bHasBaseColor = true;
    Inputs.bHasMetallic = true;
    Inputs.bHasRoughness = true;
    Inputs.bHasNormal = true;
    Inputs.bHasOcclusion = true;
    Inputs.bHasEmissive = true;
    Inputs.bHasAlpha = true;
    Inputs.ExtensionSlots = {{"ClearCoat", "0.0"}, {"Anisotropy", "0.0"}};
    return Inputs;
}

FForwardMaterialBinding Binding(Stoner::Core::uint32 MaterialId, const char* Name, EMaterialBlendMode Blend)
{
    FForwardMaterialBinding Result;
    Result.MaterialId = MaterialId;
    Result.MaterialName = Name;
    Result.Domain = EMaterialDomain::Surface;
    Result.BlendMode = Blend;
    Result.bHasMaterialBinding = true;
    Result.bHasShaderBinding = true;
    Result.SurfaceInputs = CompletePBR();
    Result.ResourceRequirements.push_back({"AlbedoTexture", FMaterialResourceReference::Texture(std::string("Textures/") + Name), Name});
    return Result;
}

FMeshDrawCandidate Draw(Stoner::Core::uint32 ObjectId,
    Stoner::Core::uint32 MeshId,
    FForwardMaterialBinding Material,
    Stoner::Core::FVector3 Position,
    bool bOpaque,
    bool bTransparent)
{
    FMeshDrawCandidate Candidate;
    Candidate.ObjectId = ObjectId;
    Candidate.MeshId = MeshId;
    Candidate.DebugName = std::string("Draw") + std::to_string(ObjectId);
    Candidate.WorldPosition = Position;
    Candidate.bWantsOpaque = bOpaque;
    Candidate.bWantsTransparent = bTransparent;
    Candidate.MaterialBinding = std::move(Material);
    return Candidate;
}

FForwardViewData View()
{
    FForwardViewData Result;
    Result.ViewName = "MainView";
    Result.ViewMatrix = Stoner::Core::FMatrix4x4::Identity();
    Result.ViewProjectionMatrix = Stoner::Core::FMatrix4x4::Identity();
    Result.CameraPosition = Stoner::Core::FVector3::Zero();
    Result.Viewport.Extent = {1280, 720};
    return Result;
}

FForwardOutputTarget Output()
{
    FForwardOutputTarget Result;
    Result.ColorTargetName = "SceneColor";
    Result.DepthTargetName = "SceneDepth";
    Result.FormatSummary = "RGBA16F";
    Result.Extent = {1280, 720};
    return Result;
}

FForwardDirectionalLight Directional(Stoner::Core::uint32 Id = 1, const char* Name = "Sun")
{
    FForwardDirectionalLight Light;
    Light.LightId = Id;
    Light.Name = Name;
    Light.Direction = Stoner::Core::FVector3(0.0f, -1.0f, -1.0f);
    Light.Color = Stoner::Core::FColor::OpaqueWhite();
    Light.Intensity = 2.0f;
    return Light;
}

FForwardPointLight Point(Stoner::Core::uint32 Id, float Z, float Intensity = 1.0f, float Range = 100.0f)
{
    FForwardPointLight Light;
    Light.LightId = Id;
    Light.Name = std::string("Point") + std::to_string(Id);
    Light.Position = Stoner::Core::FVector3(0.0f, 0.0f, Z);
    Light.Color = Stoner::Core::FColor::OpaqueWhite();
    Light.Intensity = Intensity;
    Light.Range = Range;
    return Light;
}

FForwardFrameInputs RepresentativeInputs()
{
    FForwardFrameInputs Inputs;
    Inputs.FrameName = "RepresentativeForwardFrame";
    Inputs.View = View();
    Inputs.Output = Output();
    Inputs.Environment.Mode = EForwardBackgroundMode::Sky;
    Inputs.Environment.BackgroundName = "TestSky";
    Inputs.DirectionalLights.push_back(Directional());
    for (int Index = 0; Index < 6; ++Index)
    {
        Inputs.PointLights.push_back(Point(static_cast<Stoner::Core::uint32>(10 + Index), 5.0f + static_cast<float>(Index), 2.0f));
    }
    Inputs.DrawCandidates.push_back(Draw(4, 1, Binding(2, "OpaqueB", EMaterialBlendMode::Opaque), {0.0f, 0.0f, 2.0f}, true, false));
    Inputs.DrawCandidates.push_back(Draw(1, 1, Binding(1, "OpaqueA", EMaterialBlendMode::Opaque), {0.0f, 0.0f, 1.0f}, true, false));
    Inputs.DrawCandidates.push_back(Draw(3, 2, Binding(1, "OpaqueA2", EMaterialBlendMode::Opaque), {0.0f, 0.0f, 3.0f}, true, false));
    Inputs.DrawCandidates.push_back(Draw(2, 1, Binding(3, "OpaqueC", EMaterialBlendMode::Masked), {0.0f, 0.0f, 4.0f}, true, false));
    Inputs.DrawCandidates.push_back(Draw(6, 3, Binding(4, "GlassFar", EMaterialBlendMode::Translucent), {0.0f, 0.0f, 10.0f}, false, true));
    Inputs.DrawCandidates.push_back(Draw(5, 3, Binding(5, "GlassNear", EMaterialBlendMode::Translucent), {0.0f, 0.0f, 2.0f}, false, true));
    return Inputs;
}

FForwardFramePlan Prepare(const FForwardFrameInputs& Inputs, FForwardRendererConfiguration Config = {})
{
    FForwardRenderer Renderer(Config);
    FForwardFramePlan Plan;
    (void)Renderer.PrepareFrame(Inputs, Plan);
    return Plan;
}

void TestOpaqueFramePreparation(FRendererForwardPipelineTestResult& Result)
{
    const FForwardFramePlan Plan = Prepare(RepresentativeInputs());
    Record(Result, Plan.IsValid() && Plan.AcceptedOpaqueDraws.size() == 4 && Plan.AcceptedTransparentDraws.size() == 2,
        "Forward renderer prepares representative opaque and transparent frame");
    Record(Result, Plan.FindPass(EForwardPassStage::Depth) != nullptr &&
            Plan.FindPass(EForwardPassStage::Opaque) != nullptr &&
            Plan.PassOrder[0].Stage == EForwardPassStage::Depth &&
            Plan.PassOrder[1].Stage == EForwardPassStage::Opaque,
        "Forward frame declares depth before opaque work");
    Record(Result, !Plan.GraphDeclaration.GetOutputs().empty() &&
            Plan.GraphDeclaration.GetOutputs()[0].ColorTargetName == "SceneColor",
        "Forward frame declares final color output");
    Record(Result, Plan.AcceptedOpaqueDraws[0].GetObjectId() == 1 &&
            Plan.AcceptedOpaqueDraws[1].GetObjectId() == 3,
        "Opaque draw ordering is stable by material mesh and object identity");
}

void TestInvalidViewOutputAndMaterials(FRendererForwardPipelineTestResult& Result)
{
    FForwardFrameInputs Inputs = RepresentativeInputs();
    Inputs.View.Viewport.Extent = {0, 720};
    Record(Result, Prepare(Inputs).Diagnostics.CountByCode("FWD-VIEW-EXTENT") == 1,
        "Forward renderer rejects invalid view data with deterministic diagnostic");

    Inputs = RepresentativeInputs();
    Inputs.Output.ColorTargetName.Clear();
    Record(Result, Prepare(Inputs).Diagnostics.CountByCode("FWD-OUTPUT-COLOR") == 1,
        "Forward renderer rejects missing output target");

    Inputs = RepresentativeInputs();
    Inputs.DrawCandidates.clear();
    FForwardMaterialBinding Missing = Binding(9, "MissingShader", EMaterialBlendMode::Opaque);
    Missing.bHasShaderBinding = false;
    Inputs.DrawCandidates.push_back(Draw(9, 9, Missing, {0.0f, 0.0f, 0.0f}, true, false));
    Record(Result, Prepare(Inputs).Diagnostics.CountByCode("FWD-MAT-SHADER") == 1,
        "Forward renderer rejects incomplete opaque material binding");

    Inputs.DrawCandidates.clear();
    FForwardMaterialBinding Incomplete = Binding(10, "IncompletePBR", EMaterialBlendMode::Opaque);
    Incomplete.SurfaceInputs.bHasOcclusion = false;
    Inputs.DrawCandidates.push_back(Draw(10, 10, Incomplete, {0.0f, 0.0f, 0.0f}, true, false));
    Record(Result, Prepare(Inputs).Diagnostics.CountByCode("FWD-PBR-OCCLUSION") == 1,
        "Forward renderer rejects missing full PBR-style surface input");
}

void TestLightingSelectionAndFallback(FRendererForwardPipelineTestResult& Result)
{
    FForwardFramePlan Plan = Prepare(RepresentativeInputs());
    Record(Result, Plan.LightSet.bHasDirectionalLight && Plan.LightSet.AcceptedPointLights.size() == 4 &&
            Plan.LightSet.RejectedLights.size() == 2 &&
            Plan.Diagnostics.CountByCode("FWD-POINT-LIMIT") == 1,
        "Forward renderer applies default point light limit of 4");

    FForwardRendererConfiguration Config;
    Config.PointLightLimit = 2;
    Plan = Prepare(RepresentativeInputs(), Config);
    Record(Result, Plan.LightSet.AcceptedPointLights.size() == 2 && Plan.LightSet.RejectedLights.size() == 4,
        "Forward renderer applies configurable point light limit override");

    FForwardFrameInputs Inputs = RepresentativeInputs();
    Inputs.DirectionalLights.clear();
    Inputs.PointLights = {Point(21, 20.0f, 1.0f), Point(22, 4.0f, 1.0f), Point(23, 4.0f, 3.0f)};
    Plan = Prepare(Inputs);
    Record(Result, Plan.LightSet.AcceptedPointLights.size() == 3 &&
            Plan.LightSet.AcceptedPointLights[0].LightId == 23 &&
            Plan.LightSet.AcceptedPointLights[1].LightId == 22,
        "Forward renderer sorts point lights by influence and stable identity");

    Config.PointLightLimit = 0;
    Plan = Prepare(Inputs, Config);
    Record(Result, Plan.LightSet.AcceptedPointLights.empty() &&
            Plan.AmbientFallback.bActive &&
            Plan.Diagnostics.CountByCode("FWD-AMBIENT-FALLBACK") == 1,
        "Forward renderer supports zero point light limit with ambient-only fallback");

    Inputs = RepresentativeInputs();
    Inputs.DirectionalLights.push_back(Directional(2, "SecondSun"));
    Plan = Prepare(Inputs);
    Record(Result, Plan.Diagnostics.CountByCode("FWD-DIR-MULTIPLE-PRIMARY") == 1,
        "Forward renderer reports multiple primary directional lights");

    Inputs = RepresentativeInputs();
    Inputs.PointLights.clear();
    Inputs.PointLights.push_back(Point(0, 1.0f));
    Inputs.PointLights.push_back(Point(31, 1.0f, -1.0f));
    Inputs.PointLights.push_back(Point(32, 1.0f, 1.0f, 0.0f));
    Plan = Prepare(Inputs);
    Record(Result, Plan.Diagnostics.CountByCode("FWD-POINT-ID") == 1 &&
            Plan.Diagnostics.CountByCode("FWD-POINT-INTENSITY") == 1 &&
            Plan.Diagnostics.CountByCode("FWD-POINT-RANGE") == 1,
        "Forward renderer reports invalid point light identity intensity and range");
}

void TestSkyTransparentAndDeterminism(FRendererForwardPipelineTestResult& Result)
{
    FForwardFrameInputs Inputs = RepresentativeInputs();
    FForwardFramePlan Plan = Prepare(Inputs);
    Record(Result, Plan.FindPass(EForwardPassStage::SkyBackground) != nullptr &&
            Plan.PassOrder[2].Stage == EForwardPassStage::SkyBackground &&
            Plan.PassOrder[3].Stage == EForwardPassStage::Transparent,
        "Forward renderer places sky before transparent work");
    Record(Result, Plan.AcceptedTransparentDraws[0].GetObjectId() == 6 &&
            Plan.AcceptedTransparentDraws[1].GetObjectId() == 5,
        "Forward renderer sorts transparent draws by camera-space depth descending");

    Inputs.DrawCandidates.clear();
    Inputs.DrawCandidates.push_back(Draw(12, 4, Binding(12, "TieB", EMaterialBlendMode::Translucent), {0.0f, 0.0f, 7.0f}, false, true));
    Inputs.DrawCandidates.push_back(Draw(11, 4, Binding(11, "TieA", EMaterialBlendMode::Translucent), {0.0f, 0.0f, 7.0f}, false, true));
    Plan = Prepare(Inputs);
    Record(Result, Plan.AcceptedTransparentDraws[0].GetMaterialId() == 11 &&
            Plan.AcceptedTransparentDraws[1].GetMaterialId() == 12,
        "Forward renderer breaks equal-depth transparent ties by material id");

    Inputs.DrawCandidates.clear();
    FForwardMaterialBinding OpaqueOnly = Binding(50, "OpaqueOnly", EMaterialBlendMode::Opaque);
    Inputs.DrawCandidates.push_back(Draw(50, 50, OpaqueOnly, {0.0f, 0.0f, 5.0f}, false, true));
    Plan = Prepare(Inputs);
    Record(Result, Plan.AcceptedTransparentDraws.empty() &&
            Plan.Diagnostics.CountByCode("FWD-MAT-TRANSPARENT-BLEND") == 1,
        "Forward renderer rejects incompatible transparent material");

    Inputs = RepresentativeInputs();
    Inputs.DrawCandidates.clear();
    Plan = Prepare(Inputs);
    Record(Result, Plan.IsValid() && !Plan.HasRenderableGeometry() &&
            Plan.FindPass(EForwardPassStage::SkyBackground) != nullptr,
        "Forward renderer prepares no-geometry clear or background frame");

    Inputs = RepresentativeInputs();
    const FForwardFramePlan First = Prepare(Inputs);
    bool bStable = true;
    for (int Index = 0; Index < 20; ++Index)
    {
        const FForwardFramePlan Again = Prepare(Inputs);
        bStable = bStable && Again.DebugDump == First.DebugDump;
    }
    Record(Result, bStable, "Forward frame plan diagnostics ordering and dump are stable across 20 runs");
}

void TestRepresentativePerformance(FRendererForwardPipelineTestResult& Result)
{
    const auto Start = std::chrono::steady_clock::now();
    const FForwardFramePlan Plan = Prepare(RepresentativeInputs());
    const auto End = std::chrono::steady_clock::now();
    const auto Milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start).count();
    Record(Result, Plan.IsValid() && Milliseconds < 1000,
        "Representative forward frame preparation completes under one second");
    Record(Result, Plan.DebugDump.View().find("GraphDeclaration") != std::string_view::npos &&
            Plan.DebugDump.View().find("AmbientFallback") != std::string_view::npos,
        "Forward debug dump includes graph declarations and fallback summary");
}

} // namespace

FRendererForwardPipelineTestResult RunRendererForwardPipelineTests()
{
    FRendererForwardPipelineTestResult Result;
    TestOpaqueFramePreparation(Result);
    TestInvalidViewOutputAndMaterials(Result);
    TestLightingSelectionAndFallback(Result);
    TestSkyTransparentAndDeterminism(Result);
    TestRepresentativePerformance(Result);
    return Result;
}
