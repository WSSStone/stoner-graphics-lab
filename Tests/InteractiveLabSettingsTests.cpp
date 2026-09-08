#include "FLabSettingsController.h"
#include "Renderer/FOutputTransformSettings.h"
#include <iostream>
#include <limits>

int RunInteractiveLabSettingsTests()
{
    using namespace Stoner::Application;
    using namespace Stoner::Renderer;
    int Failed = 0;
    const auto Check = [&](bool OK, const char* Name) {
        std::cout << (OK ? "[PASS] " : "[FAIL] ") << Name << '\n'; if (!OK) ++Failed;
    };
    FLabSettingsSnapshot Initial;
    Initial.CameraRevision = Initial.SettingsRevision = Initial.DisplayGeneration = Initial.OutputModeGeneration = 1;
    Initial.RequestedProfileId = Initial.EffectiveProfileId = "Sdr.sRGB.v1";
    Initial.SdrToneMapVersion = GDefaultSDRToneMapVersion;
    Initial.HdrViewingVersion = GInitialHDRViewingVersion;
    FLabSettingsCapabilities Caps;
    Caps.DisplayGeneration = 1;
    Caps.Outputs = {{"Sdr.sRGB.v1",100,100},{"Hdr.Linear.1000.v1",203,203}};
    Caps.DebugStages = {{"SceneColorHandoff",ERenderGraphColorDomain::SceneLinearRec709D65}};
    FLabSettingsController C;
    Check(C.Initialize(Initial,Caps),"settings initialize from one complete capability generation");
    auto Edit = Initial; Edit.ExposureStops = 2;
    Check(C.Request(Edit) && C.GetEffective().ExposureStops == 0 && C.GetPending()->ExposureStops == 2,
        "ordinary valid edit remains pending until eligible frame");
    Check(!C.BeginEligible(false),"busy frame does not publish settings");
    auto Bad = Edit; Bad.ExposureStops = std::numeric_limits<float>::quiet_NaN();
    Check(!C.Request(Bad) && C.GetPending()->ExposureStops == 2 && C.GetRequested().ExposureStops == 2,
        "invalid edit preserves latest valid intent and effective settings");
    Bad = Edit; Bad.RequestedProfileId = "Hdr.PQ.Rec2020.2000.v1";
    Check(!C.Request(Bad) && !C.IsPaused() && C.GetPending()->ExposureStops == 2,
        "unsupported ordinary request neither forces fallback nor replaces pending intent");
    Edit.ExposureStops = 3;
    Check(C.Request(Edit) && C.GetPending()->ExposureStops == 3,"latest valid edit supersedes one pending request");
    const auto* Active = C.BeginEligible(true);
    const auto Token = Active ? Active->Token : 0;
    Check(Active && !Active->bRequiresOutputTransition && C.GetEffective().ExposureStops == 0,
        "ordinary transaction snapshots without prematurely publishing effective state");
    Edit.ExposureStops = 4;
    Check(C.Request(Edit) && !C.BeginEligible(true),"one active transaction retains only one latest pending edit");
    Check(!C.Complete(Token+1,true,true) && C.Complete(Token,true,true) && C.GetEffective().ExposureStops == 3,
        "only matching completion atomically commits its snapshot");
    Active = C.BeginEligible(true);
    Check(Active && C.Complete(Active->Token,true,true) && C.GetEffective().ExposureStops == 4,
        "superseding pending edit follows active completion");
    Edit = C.GetEffective(); Edit.RequestedProfileId = "Hdr.Linear.1000.v1";
    Check(C.Request(Edit),"supported HDR request preserves remembered SDR strategy");
    Active = C.BeginEligible(true);
    Check(Active && Active->bRequiresOutputTransition && C.Complete(Active->Token,true,true) &&
        C.GetEffective().NativePackingWhiteNits == 203 && C.GetEffective().SdrToneMapVersion == Initial.SdrToneMapVersion,
        "HDR completion commits profile and same-generation white together");
    Caps.DisplayGeneration = 2; Caps.Outputs.resize(1);
    Check(C.RefreshCapabilities(Caps,false) && C.IsPaused() && C.GetPending() &&
        C.GetPending()->EffectiveProfileId == "Sdr.sRGB.v1" && C.GetRequested().RequestedProfileId == "Hdr.Linear.1000.v1",
        "independent HDR capability loss queues explicit SDR fallback while preserving requested HDR");
    Active = C.BeginEligible(true);
    const auto StaleToken = Active ? Active->Token : 0;
    Caps.DisplayGeneration = 3;
    Check(C.RefreshCapabilities(Caps,false) && !C.Complete(StaleToken,true,false) && C.IsPaused(),
        "new display generation invalidates old native completion");
    Active = C.BeginEligible(true);
    Check(Active && C.Complete(Active->Token,true,false) && !C.IsPaused() && C.GetEffective().DisplayGeneration == 3,
        "latest generation fallback resumes only after its own completion");
    Edit = C.GetEffective(); Edit.RequestedProfileId = "Sdr.sRGB.v1";
    Edit.DebugBypass.Mode = EOutputTransformDebugBypassMode::BoundedVisualization;
    Edit.DebugBypass.StageName = "SceneColorHandoff";
    Edit.DebugBypass.SourceDomain = ERenderGraphColorDomain::DisplayLinearRec709D65;
    Check(!C.Request(Edit),"debug stage domain mismatch rejects the complete edit");
    Edit.DebugBypass.SourceDomain = ERenderGraphColorDomain::SceneLinearRec709D65;
    Edit.DebugBypass.VisualizationMinimum = -3; Edit.DebugBypass.VisualizationMaximum = 7;
    Check(C.Request(Edit),"complete non-default debug stage and range are accepted");
    Active = C.BeginEligible(true);
    Check(Active && C.Complete(Active->Token,false,true) && !C.IsPaused() && !C.GetFailure().IsEmpty(),
        "failed replacement retains effective bindings only when former output remains usable");
    Check(C.Request(Edit),"failed edit can be explicitly retried");
    Active = C.BeginEligible(true);
    Check(Active && C.Complete(Active->Token,false,false) && C.IsPaused(),
        "retired former swapchain cannot be advertised as usable after failure");
    Caps.DisplayGeneration = 4; Caps.Outputs.clear();
    Check(C.RefreshCapabilities(Caps,false) && C.IsPaused() && !C.GetPending(),
        "no supported output pauses without manufacturing a fallback");
    Check(!C.RefreshCapabilities(Caps,false),"repeated stale capability generation rejects");
    Caps.DisplayGeneration = 5; Caps.Outputs = {{"Sdr.sRGB.v1",100,100}};
    Check(C.RefreshCapabilities(Caps,false) && C.GetPending(),"restored output capability revalidates remembered intent");
    Active = C.BeginEligible(true);
    Check(Active && C.Complete(Active->Token,true,false) && !C.IsPaused(),"capability restoration resumes after successful native handoff");
    const auto Revision = C.GetEffective().SettingsRevision;
    auto InvalidCaps = Caps; InvalidCaps.DisplayGeneration = 6;
    InvalidCaps.Outputs.push_back(InvalidCaps.Outputs.front());
    Check(!C.RefreshCapabilities(InvalidCaps,false) && !C.IsPaused() && C.GetEffective().SettingsRevision == Revision,
        "duplicate capability records cannot mutate current generation or effective state");
    InvalidCaps = Caps; InvalidCaps.DisplayGeneration = 6; InvalidCaps.Outputs[0].NativePackingWhiteNits = 203;
    Check(!C.RefreshCapabilities(InvalidCaps,false),"incoherent SDR packing white rejects entire capability refresh");
    Edit = C.GetEffective(); Edit.RequestedProfileId = "Sdr.sRGB.v1"; Edit.CameraRevision = 5;
    Check(C.Request(Edit),"new camera revision can join a complete pending settings edit");
    Bad = Edit; Bad.CameraRevision = 4;
    Check(!C.Request(Bad) && C.GetPending()->CameraRevision == 5,"stale camera edit cannot replace newer pending camera revision");
    Active = C.BeginEligible(true);
    Check(Active && C.Complete(Active->Token,true,true) && C.GetEffective().CameraRevision == 5 &&
        C.GetEffective().SettingsRevision > Revision,"camera and settings revisions commit atomically and monotonically");
    return Failed;
}
