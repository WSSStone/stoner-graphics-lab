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
    FLabSettingsController White;
    auto HdrInitial=Initial;
    HdrInitial.RequestedProfileId=HdrInitial.EffectiveProfileId="Hdr.Linear.1000.v1";
    HdrInitial.UIReferenceWhiteNits=HdrInitial.NativePackingWhiteNits=203;
    FLabSettingsCapabilities WhiteCaps;
    WhiteCaps.DisplayGeneration=1; WhiteCaps.Outputs={{"Hdr.Linear.1000.v1",203,203}};
    Check(White.Initialize(HdrInitial,WhiteCaps),"reference-white transition starts from a coherent HDR snapshot");
    WhiteCaps.DisplayGeneration=2; WhiteCaps.Outputs[0]={"Hdr.Linear.1000.v1",100,100};
    Check(White.RefreshCapabilities(WhiteCaps,true) && White.IsPaused() && White.GetPending() &&
        White.GetPending()->NativePackingWhiteNits == 100 && White.GetEffective().NativePackingWhiteNits == 203 &&
        White.GetCapabilities().DisplayGeneration == 2,
        "same-profile reference-white change pauses old packing while publishing fresh capability facts");
    const auto* WhiteTransaction=White.BeginEligible(true);
    Check(WhiteTransaction && WhiteTransaction->bRequiresOutputTransition,
        "reference-white change requires native output transition even when profile identity is unchanged");
    Check(WhiteTransaction && White.Complete(WhiteTransaction->Token,true,false) && !White.IsPaused() &&
        White.GetEffective().UIReferenceWhiteNits == 100 && White.GetEffective().NativePackingWhiteNits == 100 &&
        White.GetEffective().OutputModeGeneration == 2,
        "reference-white and packing changes become effective together after native completion");
    FLabSettingsController Fallback;
    WhiteCaps.DisplayGeneration=1;
    WhiteCaps.Outputs={{"Hdr.Linear.1000.v1",203,203},{"Sdr.sRGB.v1",100,100}};
    Check(Fallback.Initialize(HdrInitial,WhiteCaps),"fallback edit fixture starts with accepted HDR intent");
    WhiteCaps.DisplayGeneration=2; WhiteCaps.Outputs={{"Sdr.sRGB.v1",100,100}};
    Check(Fallback.RefreshCapabilities(WhiteCaps,false),"fallback edit fixture loses HDR capability");
    auto ParameterEdit=Fallback.GetRequested();
    ParameterEdit.ExposureStops=2;
    ParameterEdit.SdrToneMapVersion="Sdr.NarkowiczAcesFit.v1";
    Check(Fallback.Request(ParameterEdit) && Fallback.GetPending() &&
        Fallback.GetPending()->EffectiveProfileId == "Sdr.sRGB.v1" && Fallback.GetPending()->ExposureStops == 2 &&
        Fallback.GetRequested().RequestedProfileId == "Hdr.Linear.1000.v1",
        "parameter edit supersedes pending SDR fallback while preserving accepted unavailable HDR intent");
    auto* FallbackTransaction=Fallback.BeginEligible(true);
    Check(FallbackTransaction && Fallback.Complete(FallbackTransaction->Token,true,false) &&
        Fallback.GetEffective().ExposureStops == 2 && Fallback.GetEffective().SdrToneMapVersion == ParameterEdit.SdrToneMapVersion,
        "fallback commits the latest exposure and SDR tone map together");
    auto UnsupportedEdit=Fallback.GetRequested(); UnsupportedEdit.RequestedProfileId="Hdr.Linear.2000.v1";
    Check(!Fallback.Request(UnsupportedEdit) && Fallback.GetRequested().RequestedProfileId == "Hdr.Linear.1000.v1",
        "retaining accepted HDR intent does not authorize a different unavailable output profile");
    ParameterEdit=Fallback.GetRequested(); ParameterEdit.ExposureStops=3;
    const auto FallbackMode=Fallback.GetEffective().OutputModeGeneration;
    const bool QueuedFallbackEdit=Fallback.Request(ParameterEdit);
    FallbackTransaction=Fallback.BeginEligible(true);
    Check(QueuedFallbackEdit && FallbackTransaction && !FallbackTransaction->bRequiresOutputTransition &&
        Fallback.Complete(FallbackTransaction->Token,true,true) && Fallback.GetEffective().ExposureStops == 3 &&
        Fallback.GetEffective().OutputModeGeneration == FallbackMode,
        "ordinary edits in an effective SDR fallback do not recreate the native output");
    WhiteCaps.DisplayGeneration=3;
    WhiteCaps.Outputs={{"Hdr.Linear.1000.v1",203,203},{"Sdr.sRGB.v1",100,100}};
    const bool Restored=Fallback.RefreshCapabilities(WhiteCaps,true);
    FallbackTransaction=Fallback.BeginEligible(true);
    Check(Restored && FallbackTransaction && Fallback.Complete(FallbackTransaction->Token,true,true) &&
        Fallback.GetEffective().EffectiveProfileId == "Hdr.Linear.1000.v1" && Fallback.GetEffective().ExposureStops == 3 &&
        Fallback.GetEffective().SdrToneMapVersion == "Sdr.NarkowiczAcesFit.v1",
        "restored HDR consumes parameter edits made during fallback and retains the remembered SDR strategy");
    {
        auto ScopedCaps=Caps; ScopedCaps.DisplayGeneration=1;
        ScopedCaps.Outputs={{"Sdr.sRGB.v1",100,100},{"Hdr.Linear.1000.v1",203,203}};
        ScopedCaps.DebugStages={{"SDRToneMap",ERenderGraphColorDomain::DisplayLinearRec709D65,"Sdr.sRGB.v1"},
            {"HDRViewingTransform",ERenderGraphColorDomain::DisplayLinearRec709D65,"Hdr.Linear.1000.v1"}};
        FLabSettingsController Scoped;
        auto ScopedEdit=Initial;
        Check(Scoped.Initialize(Initial,ScopedCaps),"diagnostic stages can be scoped to output profiles");
        ScopedEdit.DebugBypass={EOutputTransformDebugBypassMode::BoundedVisualization,"HDRViewingTransform",
            ERenderGraphColorDomain::DisplayLinearRec709D65,2,8};
        Check(!Scoped.Request(ScopedEdit),"a stage from another output profile rejects despite sharing its color domain");
        ScopedEdit.RequestedProfileId="Hdr.Linear.1000.v1";
        Check(Scoped.Request(ScopedEdit),"HDR diagnostic intent is accepted with its matching output");
        auto Transaction=Scoped.BeginEligible(true);
        Check(Transaction && Scoped.Complete(Transaction->Token,true,true),"HDR diagnostic commits as one complete settings snapshot");
        ScopedCaps.DisplayGeneration=2; ScopedCaps.Outputs.resize(1);
        const bool Lost=Scoped.RefreshCapabilities(ScopedCaps,false);
        Transaction=Scoped.BeginEligible(true);
        Check(Lost && Transaction && Scoped.Complete(Transaction->Token,true,true) &&
            Scoped.GetEffective().EffectiveProfileId=="Sdr.sRGB.v1" &&
            Scoped.GetEffective().DebugBypass.Mode==EOutputTransformDebugBypassMode::Disabled &&
            Scoped.GetRequested().DebugBypass.StageName=="HDRViewingTransform",
            "SDR fallback disables an incompatible effective widget while preserving requested HDR diagnostic intent");
        ScopedCaps.DisplayGeneration=3; ScopedCaps.Outputs.push_back({"Hdr.Linear.1000.v1",203,203});
        const bool Recovered=Scoped.RefreshCapabilities(ScopedCaps,true);
        Transaction=Scoped.BeginEligible(true);
        Check(Recovered && Transaction && Scoped.Complete(Transaction->Token,true,true) &&
            Scoped.GetEffective().DebugBypass.StageName=="HDRViewingTransform" &&
            Scoped.GetEffective().DebugBypass.VisualizationMinimum==2 && Scoped.GetEffective().DebugBypass.VisualizationMaximum==8,
            "HDR recovery restores the exact requested diagnostic stage and range");
    }
    return Failed;
}
