// Feature 030 foundation value fixtures.
//
// These tests intentionally name the small value-only public API which T010
// and T011 provide.  They are written before those headers/implementations so
// an absent API is a real compile failure rather than an unconditional pass.
// The values are deliberately exercised independently: a valid camera does
// not make a display, settings, input, or UI packet valid by association.

#include "Application/FFreeCameraState.h"
#include "Application/FInputOwnershipSnapshot.h"
#include "Application/FLabSettingsSnapshot.h"
#include "Application/FWindowDisplayState.h"
#include "Renderer/FUIDrawSnapshot.h"
#include "Renderer/FUICompositionSettings.h"
#include "Renderer/FUITextureRequest.h"

#include <cmath>
#include <iostream>
#include <limits>

struct FInteractiveLabValueTestResult
{
    int Passed = 0;
    int Failed = 0;
};

namespace
{

using namespace Stoner::Application;
using namespace Stoner::Core;
using namespace Stoner::Renderer;

void Record(FInteractiveLabValueTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FMatrix4x4 MakeCameraView(const FVector3& Position, float YawRadians,
    float PitchRadians)
{
    const float CosPitch = std::cos(PitchRadians);
    const FVector3 Forward(
        CosPitch * std::cos(YawRadians),
        CosPitch * std::sin(YawRadians),
        std::sin(PitchRadians));
    const FVector3 Right(-std::sin(YawRadians), std::cos(YawRadians), 0.0f);
    const FVector3 Up = Forward.Cross(Right);
    return FMatrix4x4(
        Forward.X, Forward.Y, Forward.Z, -Forward.Dot(Position),
        Right.X, Right.Y, Right.Z, -Right.Dot(Position),
        Up.X, Up.Y, Up.Z, -Up.Dot(Position),
        0.0f, 0.0f, 0.0f, 1.0f);
}

FMatrix4x4 MakeCameraProjection(float VerticalFovRadians, float Aspect,
    float NearPlane, float FarPlane)
{
    const float VerticalScale =
        1.0f / std::tan(VerticalFovRadians * 0.5f);
    const float HorizontalScale = VerticalScale / Aspect;
    const float DepthScale = FarPlane / (FarPlane - NearPlane);
    return FMatrix4x4(
        0.0f, HorizontalScale, 0.0f, 0.0f,
        0.0f, 0.0f, -VerticalScale, 0.0f,
        DepthScale, 0.0f, 0.0f, -NearPlane * DepthScale,
        1.0f, 0.0f, 0.0f, 0.0f);
}

FFreeCameraState MakeValidCamera()
{
    FFreeCameraState Camera;
    Camera.CameraRevision = 1;
    Camera.Position = FVector3(4.0f, -2.0f, 1.0f);
    Camera.YawRadians = 0.2f;
    Camera.PitchRadians = -0.1f;
    Camera.VerticalFovRadians = 1.04719755f;
    Camera.NearPlane = 0.1f;
    Camera.FarPlane = 100.0f;
    Camera.MovementSpeed = 1.5f;
    Camera.DrawableExtent = {1280, 720};
    Camera.View = MakeCameraView(
        Camera.Position, Camera.YawRadians, Camera.PitchRadians);
    Camera.Projection = MakeCameraProjection(
        Camera.VerticalFovRadians, 1280.0f / 720.0f,
        Camera.NearPlane, Camera.FarPlane);
    Camera.ViewProjection = Camera.Projection * Camera.View;
    return Camera;
}

FWindowDisplayState MakeValidDisplay()
{
    FWindowDisplayState Display;
    Display.LogicalExtent = {1280, 720};
    Display.DrawableExtent = {2560, 1440};
    Display.ContentScale = {2.0f, 2.0f};
    Display.FramebufferScale = {2.0f, 2.0f};
    Display.DisplayGeneration = 1;
    Display.bFocused = true;
    Display.bMinimized = false;
    return Display;
}

FLabSettingsSnapshot MakeValidSettings()
{
    FLabSettingsSnapshot Settings;
    Settings.CameraRevision = 1;
    Settings.SettingsRevision = 1;
    Settings.DisplayGeneration = 1;
    Settings.OutputModeGeneration = 1;
    Settings.RequestedProfileId = "Hdr.Linear.2000.v1";
    Settings.EffectiveProfileId = "Hdr.Linear.2000.v1";
    Settings.SdrToneMapVersion = "Sdr.KhronosPbrNeutral.v1";
    Settings.HdrViewingVersion =
        "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1";
    Settings.ExposureStops = 0.0f;
    Settings.DebugBypass.StageName = "SceneColorHandoff";
    Settings.DebugBypass.Mode =
        EOutputTransformDebugBypassMode::BoundedVisualization;
    Settings.DebugBypass.SourceDomain =
        ERenderGraphColorDomain::SceneLinearRec709D65;
    Settings.DebugBypass.VisualizationMinimum = 0.0f;
    Settings.DebugBypass.VisualizationMaximum = 1.0f;
    Settings.bUIVisible = true;
    Settings.UIWhiteMultiplier = 1.0f;
    Settings.UIReferenceWhiteNits = 100.0f;
    Settings.NativePackingWhiteNits = 100.0f;
    return Settings;
}

void TestCameraFiniteAndBounds(FInteractiveLabValueTestResult& Result)
{
    FFreeCameraState Camera = MakeValidCamera();
    Record(Result, Camera.IsValid(),
        "free camera accepts finite pose projection and current drawable extent");

    Camera = MakeValidCamera();
    Camera.VerticalFovRadians = FMath::DegreesToRadians(20.0f);
    Camera.Projection = MakeCameraProjection(
        Camera.VerticalFovRadians, 1280.0f / 720.0f,
        Camera.NearPlane, Camera.FarPlane);
    Camera.ViewProjection = Camera.Projection * Camera.View;
    Record(Result, Camera.IsValid(),
        "free camera accepts the inclusive twenty degree FOV boundary");

    Camera = MakeValidCamera();
    Camera.VerticalFovRadians = FMath::DegreesToRadians(90.0f);
    Camera.Projection = MakeCameraProjection(
        Camera.VerticalFovRadians, 1280.0f / 720.0f,
        Camera.NearPlane, Camera.FarPlane);
    Camera.ViewProjection = Camera.Projection * Camera.View;
    Record(Result, Camera.IsValid(),
        "free camera accepts the inclusive ninety degree FOV boundary");

    Camera = MakeValidCamera();
    Camera.Position.X = std::numeric_limits<float>::quiet_NaN();
    Record(Result, !Camera.IsValid(),
        "free camera rejects a non-finite position component");

    Camera = MakeValidCamera();
    Camera.MovementSpeed = 0.0f;
    Record(Result, !Camera.IsValid(),
        "free camera rejects movement speed below the inclusive minimum");

    Camera = MakeValidCamera();
    Camera.MovementSpeed = 100.001f;
    Record(Result, !Camera.IsValid(),
        "free camera rejects movement speed above the v1 maximum");

    Camera = MakeValidCamera();
    Camera.VerticalFovRadians = std::nextafter(
        FMath::DegreesToRadians(20.0f), 0.0f);
    Record(Result, !Camera.IsValid(),
        "free camera rejects vertical FOV below twenty degrees");

    Camera = MakeValidCamera();
    Camera.VerticalFovRadians = std::nextafter(
        FMath::DegreesToRadians(90.0f),
        std::numeric_limits<float>::infinity());
    Record(Result, !Camera.IsValid(),
        "free camera rejects vertical FOV above ninety degrees");

    Camera = MakeValidCamera();
    Camera.NearPlane = 0.00009f;
    Record(Result, !Camera.IsValid(),
        "free camera rejects an import near plane below the v1 bound");

    Camera = MakeValidCamera();
    Camera.NearPlane = 10.0f;
    Camera.FarPlane = 10.0f;
    Record(Result, !Camera.IsValid(),
        "free camera rejects near plane equal to far plane");

    Camera = MakeValidCamera();
    Camera.FarPlane = 1000000.1f;
    Record(Result, !Camera.IsValid(),
        "free camera rejects far plane above the import maximum");

    Camera = MakeValidCamera();
    Camera.DrawableExtent = {4097, 720};
    Record(Result, !Camera.IsValid(),
        "free camera rejects a drawable axis above the v1 limit");

    Camera = MakeValidCamera();
    Camera.DrawableExtent = {0, 0};
    Record(Result, !Camera.IsValid(),
        "free camera rejects a non-renderable zero drawable extent");

    Camera = MakeValidCamera();
    Camera.CameraRevision = 0;
    Record(Result, !Camera.IsValid(),
        "free camera rejects its invalid zero revision");

    Camera = MakeValidCamera();
    Camera.ViewProjection.M[0][0] = std::numeric_limits<float>::infinity();
    Record(Result, !Camera.IsValid(),
        "free camera rejects non-finite derived matrices");
}

void TestCameraChangeIdentity(FInteractiveLabValueTestResult& Result)
{
    FCameraChangeSet Change;
    Change.CameraRevision = 2;
    Change.PreviousDrawableExtent = {1280, 720};
    Change.NewDrawableExtent = {2560, 1440};
    Record(Result, Change.IsValid(),
        "camera change set accepts a monotonic revision and finite extents");

    Change = FCameraChangeSet{};
    Change.CameraRevision = 0;
    Change.PreviousDrawableExtent = {1280, 720};
    Change.NewDrawableExtent = {2560, 1440};
    Record(Result, !Change.IsValid(),
        "camera change set rejects an invalid zero revision");

    Change = FCameraChangeSet{};
    Change.CameraRevision = 3;
    Change.PreviousDrawableExtent = {1280, 720};
    Change.NewDrawableExtent = {4097, 720};
    Record(Result, !Change.IsValid(),
        "camera change set rejects an extent above the drawable bound");
}

void TestDisplayFiniteGenerationAndScale(
    FInteractiveLabValueTestResult& Result)
{
    FWindowDisplayState Display = MakeValidDisplay();
    Record(Result, Display.IsValid(),
        "window display state accepts logical extent drawable extent and scale");

    Display = MakeValidDisplay();
    Display.DisplayGeneration = 0;
    Record(Result, !Display.IsValid(),
        "window display state rejects an invalid zero generation");

    Display = MakeValidDisplay();
    Display.ContentScale = {0.49f, 2.0f};
    Record(Result, !Display.IsValid(),
        "window display state rejects content scale below the supported range");

    Display = MakeValidDisplay();
    Display.ContentScale = {4.01f, 2.0f};
    Record(Result, !Display.IsValid(),
        "window display state rejects content scale above the supported range");

    Display = MakeValidDisplay();
    Display.FramebufferScale = {1.0f, 1.0f};
    Record(Result, !Display.IsValid(),
        "window display state rejects a framebuffer scale that disagrees with drawable ratio");

    Display = MakeValidDisplay();
    Display.LogicalExtent = {0, 720};
    Record(Result, !Display.IsValid(),
        "window display state rejects a zero logical client axis");

    Display = MakeValidDisplay();
    Display.LogicalExtent = {8192, 2000};
    Display.DrawableExtent = {4096, 1000};
    Display.ContentScale = {0.5f, 0.5f};
    Display.FramebufferScale = {0.5f, 0.5f};
    Record(Result, Display.IsValid(),
        "window display state bounds drawable pixels while preserving a larger logical extent");

    Display = MakeValidDisplay();
    Display.DrawableExtent = {4096, 1921};
    Record(Result, !Display.IsValid(),
        "window display state rejects drawable pixel count above the v1 cap");

    Display = MakeValidDisplay();
    Display.DrawableExtent = {0, 0};
    Record(Result, Display.IsValid(),
        "window display state represents a zero drawable without requiring minimized state");

    Display = MakeValidDisplay();
    Display.bMinimized = true;
    Record(Result, Display.IsValid(),
        "window display state preserves a minimized native size while paused state is independent");
}

void TestInputOwnershipIdentity(FInteractiveLabValueTestResult& Result)
{
    FInputOwnershipSnapshot Ownership;
    Ownership.EventSequence = 7;
    Ownership.FocusGeneration = 3;
    Ownership.bFocused = true;
    Ownership.bPointerBaselineValid = true;
    Record(Result, Ownership.IsValid(),
        "input ownership accepts nonzero event and focus generations");

    Ownership = FInputOwnershipSnapshot{};
    Ownership.EventSequence = 0;
    Ownership.FocusGeneration = 3;
    Record(Result, !Ownership.IsValid(),
        "input ownership rejects an invalid zero event sequence");

    Ownership = FInputOwnershipSnapshot{};
    Ownership.EventSequence = 7;
    Ownership.FocusGeneration = 0;
    Record(Result, !Ownership.IsValid(),
        "input ownership rejects an invalid zero focus generation");

    Ownership = FInputOwnershipSnapshot{};
    Ownership.EventSequence = 7;
    Ownership.FocusGeneration = 3;
    Ownership.bFocused = true;
    Ownership.bPointerBaselineValid = true;
    const FInputOwnershipSnapshot Copy = Ownership;
    Record(Result, Copy.EventSequence == 7 &&
            Copy.FocusGeneration == 3 && Copy.bFocused &&
            Copy.bPointerBaselineValid,
        "input ownership copies generation and baseline values independently");
}

void TestSettingsFiniteGenerationAndDebug(
    FInteractiveLabValueTestResult& Result)
{
    FLabSettingsSnapshot Settings = MakeValidSettings();
    Record(Result, Settings.IsValid(),
        "lab settings accept complete profile version debug and UI values");

    Settings = MakeValidSettings();
    Settings.SettingsRevision = 0;
    Record(Result, !Settings.IsValid(),
        "lab settings reject an invalid zero settings revision");

    Settings = MakeValidSettings();
    Settings.CameraRevision = 0;
    Record(Result, !Settings.IsValid(),
        "lab settings reject a stale invalid zero camera revision");

    Settings = MakeValidSettings();
    Settings.DisplayGeneration = 0;
    Record(Result, !Settings.IsValid(),
        "lab settings reject an invalid zero display generation");

    Settings = MakeValidSettings();
    Settings.ExposureStops = -16.001f;
    Record(Result, !Settings.IsValid(),
        "lab settings reject exposure below the v1 range");

    Settings = MakeValidSettings();
    Settings.ExposureStops = 16.001f;
    Record(Result, !Settings.IsValid(),
        "lab settings reject exposure above the v1 range");

    Settings = MakeValidSettings();
    Settings.ExposureStops = std::numeric_limits<float>::infinity();
    Record(Result, !Settings.IsValid(),
        "lab settings reject non-finite exposure");

    Settings = MakeValidSettings();
    Settings.UIWhiteMultiplier = 0.249f;
    Record(Result, !Settings.IsValid(),
        "lab settings reject UI white multiplier below the v1 range");

    Settings = MakeValidSettings();
    Settings.UIWhiteMultiplier = 2.001f;
    Record(Result, !Settings.IsValid(),
        "lab settings reject UI white multiplier above the v1 range");

    Settings = MakeValidSettings();
    Settings.DebugBypass.VisualizationMinimum = 2.0f;
    Settings.DebugBypass.VisualizationMaximum = 1.0f;
    Record(Result, !Settings.IsValid(),
        "lab settings reject an inverted complete debug visualization range");

    Settings = MakeValidSettings();
    Settings.DebugBypass.VisualizationMaximum =
        std::numeric_limits<float>::quiet_NaN();
    Record(Result, !Settings.IsValid(),
        "lab settings reject non-finite debug visualization bounds");

    Settings = MakeValidSettings();
    Settings.DebugBypass.VisualizationMinimum =
        -std::numeric_limits<float>::max();
    Settings.DebugBypass.VisualizationMaximum =
        std::numeric_limits<float>::max();
    Record(Result, !Settings.IsValid(),
        "lab settings reject a debug range whose float32 difference overflows");

    Settings = MakeValidSettings();
    Settings.DebugBypass.SourceDomain = ERenderGraphColorDomain::Unspecified;
    Record(Result, !Settings.IsValid(),
        "lab settings reject an unspecified complete debug source domain");

    Settings = MakeValidSettings();
    Settings.NativePackingWhiteNits = 0.0f;
    Record(Result, !Settings.IsValid(),
        "lab settings reject a missing native packing white");

    Settings = MakeValidSettings();
    Settings.RequestedProfileId = "Hdr.Profile.DoesNotExist.v1";
    Record(Result, !Settings.IsValid(),
        "lab settings reject an output profile absent from the existing registry");

    Settings = MakeValidSettings();
    Settings.SdrToneMapVersion = Settings.HdrViewingVersion;
    Record(Result, !Settings.IsValid(),
        "lab settings reject an HDR viewing identity in the SDR strategy slot");

    Settings = MakeValidSettings();
    Settings.HdrViewingVersion = Settings.SdrToneMapVersion;
    Record(Result, !Settings.IsValid(),
        "lab settings reject an SDR tone-map identity in the HDR strategy slot");

    Settings = MakeValidSettings();
    Record(Result,
        Settings.DebugBypass.IsValidForResolvedStageDomain(
            "SceneColorHandoff", ERenderGraphColorDomain::SceneLinearRec709D65),
        "lab settings accept a freshly resolved matching debug stage and domain");
    Record(Result,
        !Settings.DebugBypass.IsValidForResolvedStageDomain(
            "SceneColorHandoff", ERenderGraphColorDomain::DisplayLinearRec709D65),
        "lab settings reject a debug domain mismatch after stage resolution");
}

void TestCopiedRendererValues(FInteractiveLabValueTestResult& Result)
{
    FUIVertex SourceVertex;
    SourceVertex.Position = {12.5f, 24.0f};
    SourceVertex.UV = {0.25f, 0.75f};
    const FUIVertex CopiedVertex = SourceVertex;
    SourceVertex.Position.X = 99.0f;
    SourceVertex.UV.Y = 99.0f;
    Record(Result, CopiedVertex.Position.X == 12.5f &&
            CopiedVertex.Position.Y == 24.0f &&
            CopiedVertex.UV.X == 0.25f && CopiedVertex.UV.Y == 0.75f,
        "copied UI vertex owns logical position and UV values");

    FUIDrawCommand SourceCommand;
    SourceCommand.FirstIndex = 17;
    SourceCommand.IndexCount = 6;
    SourceCommand.BaseVertex = -3;
    const FUIDrawCommand CopiedCommand = SourceCommand;
    SourceCommand.FirstIndex = 99;
    SourceCommand.BaseVertex = 12;
    Record(Result, CopiedCommand.FirstIndex == 17 &&
            CopiedCommand.IndexCount == 6 && CopiedCommand.BaseVertex == -3,
        "copied UI command owns nonzero index and signed vertex offsets");

    FUITextureId Texture;
    Texture.Slot = 9;
    Texture.Generation = 4;
    Record(Result, Texture.IsValid(),
        "UI texture identity accepts a nonzero slot and generation");
    const FUITextureId CopiedTexture = Texture;
    Record(Result, CopiedTexture.Slot == 9 && CopiedTexture.Generation == 4,
        "copied UI texture identity preserves slot and generation");

    Texture.Generation = 0;
    Record(Result, !Texture.IsValid(),
        "UI texture identity rejects a stale zero generation");
    Texture = FUITextureId{};
    Texture.Generation = 4;
    Record(Result, !Texture.IsValid(),
        "UI texture identity rejects an invalid zero slot");

    FUICompositionSettings Composition;
    Composition.OutputProfileId = "Hdr.Linear.2000.v1";
    Composition.BlendDomain =
        ERenderGraphColorDomain::DisplayLinearRec709D65;
    Composition.UIWhiteMultiplier = 1.25f;
    Composition.UIReferenceWhiteNits = 203.0f;
    Composition.NativePackingWhiteNits = 203.0f;
    Composition.DisplayGeneration = 5;
    Record(Result, Composition.IsValid(),
        "copied UI composition accepts same-generation reference and packing white");
    const FUICompositionSettings CopiedComposition = Composition;
    Record(Result, CopiedComposition.OutputProfileId == "Hdr.Linear.2000.v1" &&
            CopiedComposition.BlendDomain ==
                ERenderGraphColorDomain::DisplayLinearRec709D65 &&
            CopiedComposition.UIWhiteMultiplier == 1.25f &&
            CopiedComposition.UIReferenceWhiteNits == 203.0f &&
            CopiedComposition.NativePackingWhiteNits == 203.0f &&
            CopiedComposition.DisplayGeneration == 5,
        "copied UI composition preserves white and display-generation values");

    Composition.NativePackingWhiteNits = 202.0f;
    Record(Result, !Composition.IsValid(),
        "UI composition rejects a mismatched same-generation packing white");
    Composition = CopiedComposition;
    Composition.OutputProfileId = "Sdr.sRGB.v1";
    Record(Result, !Composition.IsValid(),
        "UI composition rejects EDR white values under an SDR profile");
    Composition = CopiedComposition;
    Composition.DisplayGeneration = 0;
    Record(Result, !Composition.IsValid(),
        "UI composition rejects an invalid zero display generation");

    FUIDrawSnapshot Snapshot;
    TArray<FUITextureId> Generations;
    for (uint64 Generation = 1; Generation <= 512; ++Generation)
        Generations.push_back({1, Generation});
    Record(Result, Snapshot.SetTextureIds(Generations),
        "snapshot can retain multiple generations of a logical texture slot");
    Generations.push_back({1, 513});
    Record(Result, !Snapshot.SetTextureIds(Generations) &&
            Snapshot.GetTextureIds().size() == 512,
        "snapshot rejects excess generation references without replacing prior values");

    FUITextureResult TextureResult;
    TextureResult.RequestId = 1;
    TextureResult.State = EUITextureState::Ready;
    Record(Result, !TextureResult.IsValid() && !TextureResult.Succeeded(),
        "failed texture result cannot describe a ready generation");
    TextureResult.Result = Stoner::RHI::ERHIResult::NotReady;
    TextureResult.State = EUITextureState::Requested;
    Record(Result, TextureResult.IsValid() && !TextureResult.Succeeded(),
        "pending texture request remains distinct from successful preparation");
}

} // namespace

int RunInteractiveLabValueTests()
{
    FInteractiveLabValueTestResult Result;
    std::cout << "[INFO] Running interactive-lab value tests\n";
    TestCameraFiniteAndBounds(Result);
    TestCameraChangeIdentity(Result);
    TestDisplayFiniteGenerationAndScale(Result);
    TestInputOwnershipIdentity(Result);
    TestSettingsFiniteGenerationAndDebug(Result);
    TestCopiedRendererValues(Result);
    std::cout << "[INFO] Interactive-lab value tests passed="
              << Result.Passed << " failed=" << Result.Failed << '\n';
    return Result.Failed == 0 ? 0 : 1;
}
