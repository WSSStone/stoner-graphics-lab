#include "ProductionCameraPreviewTests.h"

#include "Application/FInputEvent.h"
#include "FProductionCameraPreset.h"
#include "FProductionCameraPreview.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{

using namespace Stoner;
using namespace Stoner::Application;
using namespace Stoner::Demo;

void Record(
    FProductionCameraPreviewTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

Core::TArray<FInputEvent> NavigationEvents()
{
    return {
        FInputEvent::PointerMove(100.0f, 100.0f, 1),
        FInputEvent::MouseDown(EMouseButton::Right, 2),
        FInputEvent::PointerMove(132.0f, 84.0f, 3),
        FInputEvent::KeyDown(EKey::W, 4),
        FInputEvent::KeyDown(EKey::D, 5),
        FInputEvent::KeyDown(EKey::LeftShift, 6),
        FInputEvent::Scroll(0.0f, 2.0f, 7),
    };
}

Application::FWindowDisplayState DisplayState(
    Core::uint32 Width,
    Core::uint32 Height,
    Core::uint64 Generation = 1)
{
    Application::FWindowDisplayState Display;
    Display.LogicalExtent = {Width, Height};
    Display.DrawableExtent = {Width, Height};
    Display.ContentScale = {1.0f, 1.0f};
    Display.FramebufferScale = {1.0f, 1.0f};
    Display.DisplayGeneration = Generation;
    Display.bFocused = true;
    Display.bMinimized = false;
    return Display;
}

const std::string& FrozenSponzaCandidateBytes()
{
    static const std::string Bytes =
        "{\"schema\":\"stoner.production-camera-candidate\","
        "\"schemaVersion\":1,"
        "\"workloadRevision\":\"production-content-sponza-v2\","
        "\"backend\":\"metal\",\"width\":512,\"height\":512,"
        "\"view\":[-0.136053622,-0.986105621,-0.0953160524,1.61162198,"
        "0.990615845,-0.136675894,0,-3.93525147,"
        "-0.0130274063,-0.0944215953,0.995447159,0.12801826,"
        "0,0,0,1],\"projection\":[0,1.7320509,0,0,0,0,-1.7320509,0,"
        "1.001001,0,0,-0.1001001,1,0,0,0],"
        "\"matrixSha256\":\"34ac58edc10864b3294d6a97c41101bbd006dce5313715e49ad14547a7eab8a7\"}\n";
    return Bytes;
}

} // namespace

FProductionCameraPreviewTestResult RunProductionCameraPreviewTests()
{
    FProductionCameraPreviewTestResult Result;
    Core::FString Reason;
    FProductionCameraPreset Lantern;
    Record(Result,
        ResolveProductionCameraPreset(
            "production-content-lantern-v2", Lantern, &Reason) &&
            Lantern.IsValid() &&
            Lantern.CameraPosition == Core::FVector3::Zero() &&
            Lantern.ViewProjection.NearlyEquals(
                Lantern.Projection * Lantern.View) &&
            (Lantern.InverseViewProjection * Lantern.ViewProjection)
                .NearlyEquals(Core::FMatrix4x4::Identity(), 1.0e-4f),
        "camera preset derives finite camera and inverse matrices");

    FProductionCameraPreset Missing;
    FProductionCameraPreset SponzaV2;
    Record(Result,
        ResolveProductionCameraPreset(
            "production-content-sponza-v2", SponzaV2, &Reason) &&
            SponzaV2.IsValid() &&
            Core::FMath::IsNearlyEqual(
                SponzaV2.View.M[0][0], -0.136053622f) &&
            Core::FMath::IsNearlyEqual(
                SponzaV2.View.M[1][3], -3.93525147f) &&
            Core::FMath::IsNearlyEqual(
                SponzaV2.Projection.M[0][1], 1.7320509f),
        "Sponza v2 resolves the explicitly accepted frozen matrices");
    Record(Result,
        !ResolveProductionCameraPreset(
            "production-content-unknown", Missing, &Reason) &&
            !Missing.IsValid(),
        "camera preset lookup rejects undeclared workload revision");

    Core::FMatrix4x4 ScaledView = Core::FMatrix4x4::Identity();
    ScaledView.M[1][1] = 2.0f;
    Record(Result,
        !BuildProductionCameraPreset(
            "invalid-view", ScaledView, Lantern.Projection,
            Missing, &Reason),
        "camera preset rejects scaled or non-orthonormal View");
    Record(Result,
        !BuildProductionCameraPreset(
            "invalid-projection", Core::FMatrix4x4::Identity(),
            Core::FMatrix4x4::Identity(), Missing, &Reason),
        "camera preset rejects a non-perspective Projection");

    FProductionCameraPreviewController Frozen;
    const bool bFrozenInitialized = Frozen.Initialize(
        SponzaV2, 512, 512, &Reason);
    const FProductionCameraCandidate FrozenBefore =
        Frozen.BuildCandidate("metal", "production-content-sponza-v2");
    const auto FrozenNoOp = Frozen.Update({}, 0.0);
    const FProductionCameraCandidate FrozenAfter =
        Frozen.BuildCandidate("metal", "production-content-sponza-v2");
    Record(Result,
        bFrozenInitialized && !FrozenNoOp.bCameraChanged &&
            Frozen.GetCamera().View.NearlyEquals(SponzaV2.View, 1.0e-6f) &&
            Frozen.GetCamera().Projection.NearlyEquals(
                SponzaV2.Projection, 1.0e-6f) &&
            FrozenBefore.MatrixSha256 ==
                "34ac58edc10864b3294d6a97c41101bbd006dce5313715e49ad14547a7eab8a7" &&
            FrozenBefore.CanonicalJson.ToStdString() ==
                FrozenSponzaCandidateBytes() &&
            FrozenBefore.CanonicalJson == FrozenAfter.CanonicalJson &&
            FrozenBefore.MatrixSha256 == FrozenAfter.MatrixSha256,
        "an untouched frozen preset preserves candidate matrix bytes");

    FProductionCameraPreviewController First;
    FProductionCameraPreviewController Second;
    const bool bInitialized =
        First.Initialize(Lantern, 512, 512, &Reason) &&
        Second.Initialize(Lantern, 512, 512, &Reason);
    const auto FirstUpdate = First.Update(NavigationEvents(), 0.125);
    const auto SecondUpdate = Second.Update(NavigationEvents(), 0.125);
    Record(Result,
        bInitialized && FirstUpdate.bCameraChanged &&
            SecondUpdate.bCameraChanged &&
            First.GetCamera().View.NearlyEquals(Second.GetCamera().View) &&
            First.GetCamera().Projection.NearlyEquals(
                Second.GetCamera().Projection) &&
            !First.GetCamera().View.NearlyEquals(Lantern.View) &&
            !First.GetCamera().Projection.NearlyEquals(Lantern.Projection),
        "identical free-camera events and delta produce identical matrices");

    bool bAllTranslationControlsMove = true;
    for (const EKey Key : {EKey::W, EKey::S, EKey::A,
             EKey::D, EKey::Q, EKey::E})
    {
        FProductionCameraPreviewController Translation;
        const bool bReady = Translation.Initialize(
            Lantern, 512, 512, &Reason);
        const auto Move = Translation.Update(
            {FInputEvent::KeyDown(Key)}, 0.125);
        bAllTranslationControlsMove = bAllTranslationControlsMove &&
            bReady && Move.bCameraChanged &&
            !Translation.GetCamera().View.NearlyEquals(Lantern.View);
    }
    Record(Result, bAllTranslationControlsMove,
        "W S A D Q E each move the calibration camera");

    FProductionCameraPreviewController NormalSpeed;
    FProductionCameraPreviewController FastSpeed;
    const bool bSpeedReady = NormalSpeed.Initialize(
            Lantern, 512, 512, &Reason) &&
        FastSpeed.Initialize(Lantern, 512, 512, &Reason);
    (void)NormalSpeed.Update({FInputEvent::KeyDown(EKey::W)}, 0.125);
    (void)FastSpeed.Update({FInputEvent::KeyDown(EKey::W),
        FInputEvent::KeyDown(EKey::LeftShift)}, 0.125);
    Record(Result,
        bSpeedReady && FastSpeed.GetCamera().CameraPosition.X >
            NormalSpeed.GetCamera().CameraPosition.X * 3.9f,
        "Shift accelerates held preview translation");

    const auto SnapshotUpdate = First.Update(
        {FInputEvent::KeyDown(EKey::Enter, 8)}, 0.0);
    const auto Candidate = First.BuildCandidate(
        "metal", "production-content-sponza-v2");
    Record(Result,
        SnapshotUpdate.bSnapshotRequested && Candidate.IsValid() &&
            Candidate.CanonicalJson.ToStdString().find(
                "\"view\":[") != std::string::npos &&
            Candidate.CanonicalJson.ToStdString().find(
                "\"projection\":[") != std::string::npos &&
            Candidate.MatrixSha256.Len() == 64,
        "camera snapshot emits bounded canonical View and Projection evidence");

    const Core::FString CandidatePath =
        "Build/Validation/028/tests/camera-candidate.json";
    std::ifstream Existing(CandidatePath.CStr(), std::ios::binary);
    (void)Existing;
    Record(Result,
        WriteProductionCameraCandidate(CandidatePath, Candidate, &Reason),
        "camera candidate writer persists explicit calibration output");
    std::ifstream CandidateFile(CandidatePath.CStr(), std::ios::binary);
    const std::string CandidateText{
        std::istreambuf_iterator<char>(CandidateFile),
        std::istreambuf_iterator<char>()};
    Record(Result,
        CandidateFile.good() || CandidateFile.eof(),
        "camera candidate output is readable");
    Record(Result,
        CandidateText == Candidate.CanonicalJson.ToStdString(),
        "camera candidate output preserves canonical bytes");

    const auto ResetUpdate = First.Update(
        {FInputEvent::KeyDown(EKey::R, 9)}, 0.0);
    Record(Result,
        ResetUpdate.bCameraChanged &&
            First.GetCamera().View.NearlyEquals(Lantern.View) &&
            First.GetCamera().Projection.NearlyEquals(Lantern.Projection),
        "camera reset restores the exact workload preset");

    FProductionCameraPreviewController ChangedAspect;
    const bool bChangedAspectInitialized = ChangedAspect.Initialize(
        Lantern, 512, 512, &Reason);
    const auto AspectUpdate = ChangedAspect.Update(
        {}, 0.0, DisplayState(1024, 768, 2));
    const auto AspectCandidate = ChangedAspect.BuildCandidate(
        "metal", "production-content-lantern-v2");
    Record(Result,
        bChangedAspectInitialized && AspectUpdate.bCameraChanged &&
            ChangedAspect.GetCamera().View.NearlyEquals(Lantern.View) &&
            !ChangedAspect.GetCamera().Projection.NearlyEquals(
                Lantern.Projection) &&
            AspectCandidate.Width == 512 && AspectCandidate.Height == 512,
        "interactive drawable aspect rebuilds projection without changing export context");

    FProductionCameraPreviewController NonSquareInitial;
    const bool bNonSquareInitialized = NonSquareInitial.Initialize(
        Lantern, 1024, 768, &Reason);
    const auto NonSquareCandidate = NonSquareInitial.BuildCandidate(
        "metal", "production-content-lantern-v2");
    Record(Result,
        bNonSquareInitialized &&
            NonSquareInitial.GetCamera().View.NearlyEquals(Lantern.View) &&
            !NonSquareInitial.GetCamera().Projection.NearlyEquals(
                Lantern.Projection) &&
            NonSquareCandidate.Width == 1024 &&
            NonSquareCandidate.Height == 768,
        "interactive initialization adopts a non-square drawable aspect");

    FProductionCameraPreviewController OrderedLens;
    const bool bOrderedLensInitialized = OrderedLens.Initialize(
        Lantern, 512, 512, &Reason);
    const auto OrderedLensUpdate = OrderedLens.Update({
        FInputEvent::Scroll(0.0f, 100.0f, 20),
        FInputEvent::Scroll(0.0f, -100.0f, 21)}, 0.0);
    Record(Result,
        bOrderedLensInitialized && OrderedLensUpdate.bCameraChanged &&
            Core::FMath::IsNearlyEqual(
                OrderedLens.GetCamera().Projection.M[1][2], -1.0f,
                1.0e-4f),
        "ordered wheel events preserve sequential FOV clamping");

    FProductionCameraPreviewController LookRelease;
    const bool bLookReleaseInitialized = LookRelease.Initialize(
        Lantern, 512, 512, &Reason);
    const auto LookReleaseUpdate = LookRelease.Update({
        FInputEvent::PointerMove(100.0f, 100.0f, 30),
        FInputEvent::MouseDown(EMouseButton::Right, 31),
        FInputEvent::PointerMove(140.0f, 100.0f, 32),
        FInputEvent::MouseUp(EMouseButton::Right, 33)}, 0.0);
    Record(Result,
        bLookReleaseInitialized && LookReleaseUpdate.bCameraChanged &&
            !LookRelease.IsLookCaptured() &&
            !LookRelease.GetCamera().View.NearlyEquals(Lantern.View),
        "a final captured look delta survives same-batch mouse release");

    FProductionCameraPreviewController ExactReset;
    const bool bExactResetInitialized = ExactReset.Initialize(
        SponzaV2, 512, 512, &Reason);
    const FProductionCameraCandidate BeforeExactReset =
        ExactReset.BuildCandidate("metal", "production-content-sponza-v2");
    (void)ExactReset.Update(NavigationEvents(), 0.125);
    (void)ExactReset.Update(
        {FInputEvent::KeyDown(EKey::R, 34)}, 0.0);
    const FProductionCameraCandidate AfterExactReset =
        ExactReset.BuildCandidate("metal", "production-content-sponza-v2");
    Record(Result,
        bExactResetInitialized &&
            BeforeExactReset.CanonicalJson == AfterExactReset.CanonicalJson &&
            BeforeExactReset.MatrixSha256 == AfterExactReset.MatrixSha256,
        "reset restores the original frozen candidate bytes at its aspect");

    const Core::FMatrix4x4 BeforeFocusLoss = First.GetCamera().View;
    const auto FocusUpdate = First.Update({
        FInputEvent::KeyDown(EKey::W, 10),
        FInputEvent::FocusLost(11)}, 0.25);
    Record(Result,
        !FocusUpdate.bCameraChanged &&
            First.GetCamera().View.NearlyEquals(BeforeFocusLoss),
        "focus loss clears preview movement and mouse state");

    FProductionCameraPreviewController Resumed;
    const bool bResumedInitialized = Resumed.Initialize(
        Lantern, 512, 512, &Reason);
    const auto ResumeLoss = Resumed.Update({
        FInputEvent::KeyDown(EKey::W, 40),
        FInputEvent::FocusLost(41)}, 0.25, DisplayState(512, 512, 3));
    const Core::FVector3 BeforeResume = Resumed.GetCamera().CameraPosition;
    const auto ResumeBatch = Resumed.Update({
        FInputEvent::Scroll(0.0f, 1.0f, 42),
        FInputEvent::KeyDown(EKey::W, 43)}, 0.25,
        DisplayState(512, 512, 4));
    const Core::FVector3 DuringResume = Resumed.GetCamera().CameraPosition;
    const auto ResumeNext = Resumed.Update({}, 0.25,
        DisplayState(512, 512, 4));
    Record(Result,
        bResumedInitialized && !ResumeLoss.bCameraChanged &&
            ResumeBatch.bCameraChanged &&
            DuringResume == BeforeResume &&
            ResumeNext.bCameraChanged &&
            Resumed.GetCamera().CameraPosition != BeforeResume,
        "focus resume applies lens changes before allowing translation");

    const auto ExitUpdate = First.Update(
        {FInputEvent::KeyDown(EKey::Escape, 12)}, 0.0);
    Record(Result, ExitUpdate.bExitRequested,
        "Escape requests calibration preview exit");
    return Result;
}
