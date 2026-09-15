#include "Application/FLabPreset.h"
#include "Application/FLabPresetStorage.h"
#include "FInteractiveLabRun.h"
#include "FLabCaptureExport.h"
#include "FProductionContentRuntime.h"

#include "Application/FInteractiveLabSession.h"
#include "Application/FLabSettingsSnapshot.h"
#include "Asset/FAssetCookContractCodec.h"
#include "Core/FPlatformFileSystem.h"
#include "FLabProductionPreviewExecutor.h"
#include "FProductionContentSession.h"
#include "FInteractiveLabShaders.h"
#include "Renderer/FUIRenderSession.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <thread>

#define STONER_LAB_STRINGIFY_VALUE(Value) #Value
#define STONER_LAB_STRINGIFY(Value) STONER_LAB_STRINGIFY_VALUE(Value)

namespace Stoner::Demo
{
namespace
{
using RHI::ERHIResult;
using Application::EInteractiveLabServicePhase;
using Application::EInteractiveLabServiceStatus;
using Application::EInteractiveLabSessionState;
using Application::FInteractiveLabServiceResponse;
using Application::FWindowExtent;
using Clock = std::chrono::steady_clock;

bool ConfigureLabExports(Application::FInteractiveLabSession& Session, const FDemoConfiguration& Config,
    Core::FString& Reason, Application::FLabPresetStoreConfig* CaptureStore=nullptr)
{
    Application::FLabPresetStoreConfig Store;
    Core::TArray<Core::FString> Protected = {"Content","Config","Validation","Build/Validation",
        "Build/Content","Build/ProductionContent",Config.CookedPublicationRoot};
    if (!Config.BaselineRoot.IsEmpty()) Protected.push_back(Config.BaselineRoot);
    for (const auto& Path : Protected)
    {
        if (!Core::FPlatformFileSystem::Exists(Path)) continue;
        Core::FString Canonical;
        if (!Core::FPlatformFileSystem::CanonicalizeExistingPath(Path,Canonical).IsSuccess())
        { Reason="Cannot resolve protected preset destination"; return false; }
        bool Inside=false;
        if (!Core::FPlatformFileSystem::CheckContainedPath(Canonical,Config.LabExportRoot,Inside).IsSuccess() || Inside)
        { Reason="Preset export root is inside protected content or evidence"; return false; }
        if (std::find(Store.ProtectedPaths.begin(),Store.ProtectedPaths.end(),Canonical)==Store.ProtectedPaths.end())
            Store.ProtectedPaths.push_back(Canonical);
    }
    if (!Core::FPlatformFileSystem::CreateDirectory(Config.LabExportRoot) ||
        !Core::FPlatformFileSystem::CanonicalizeExistingPath(Config.LabExportRoot,Store.ExportRoot).IsSuccess())
    { Reason="Cannot create or resolve preset export directory"; return false; }
    Application::FLabPresetSourceContext Context;
    Context.Backend=Config.GraphicsBackend==EDemoGraphicsBackend::Metal ? "Metal" : "Vulkan";
    Context.CookedGeneration=Config.StrictGeneration;
    Context.SoftwareRevision=STONER_LAB_STRINGIFY(STONER_DEMO_SOFTWARE_REVISION);
    if (!Session.ConfigurePresetExports(Store,Context))
    { Reason="Cannot configure preset export provenance or protection"; return false; }
    if (CaptureStore) *CaptureStore=Store;
    return true;
}

bool Pending(ERHIResult Result) noexcept
{
    return Result == ERHIResult::NotReady;
}

bool ValidExtent(FWindowExtent Extent) noexcept
{
    return Extent.IsPositive() && Extent.Width <= FLabProductionFrameLimits::MaxDrawableAxis &&
        Extent.Height <= FLabProductionFrameLimits::MaxDrawableAxis &&
        static_cast<Core::uint64>(Extent.Width) * Extent.Height <=
            FLabProductionFrameLimits::MaxDrawablePixels;
}

RHI::FRHISwapchainDesc PresentationRequest(
    const Renderer::FResolvedOutputTransformSettings& Settings, FWindowExtent Extent)
{
    RHI::FRHISwapchainDesc Request;
    Request.Width = Extent.Width; Request.Height = Extent.Height;
    Request.FramesInFlight = 2;
    Request.PreferredFormat = Settings.OutputFormat;
    Request.PreferredColorSpace = Settings.ColorSpace;
    Request.NativeEncoding = Settings.NativeEncoding;
    Request.ReferenceWhiteNits = Settings.ReferenceWhiteNits;
    Request.TargetPeakNits = Settings.TargetPeakNits;
    Request.DisplayAdaptation = Settings.NativeEncoding == RHI::ERHIPresentationNativeEncoding::Pq
        ? RHI::ERHIPresentationDisplayAdaptation::SystemColorManagement
        : RHI::ERHIPresentationDisplayAdaptation::None;
    Request.bHasHDRMetadata = false;
    return Request;
}

bool InitialCamera(const FProductionCameraPreset& Preset, FWindowExtent Extent,
    Application::FFreeCameraState& Out)
{
    if (!Preset.IsValid() || !ValidExtent(Extent)) return false;
    const Core::FVector3 Forward(Preset.View.M[0][0], Preset.View.M[0][1], Preset.View.M[0][2]);
    const float Scale = -Preset.Projection.M[1][2];
    if (!std::isfinite(Scale) || Scale <= 0) return false;
    Out = {};
    Out.CameraRevision = 1;
    Out.Position = Preset.CameraPosition;
    Out.YawRadians = std::atan2(Forward.Y, Forward.X);
    Out.PitchRadians = std::asin(std::clamp(Forward.Z, -1.0f, 1.0f));
    Out.VerticalFovRadians = 2.0f * std::atan(1.0f / Scale);
    Out.DrawableExtent = Extent;
    Out.View = Preset.View;
    Out.Projection = MakeProductionPerspective(Out.VerticalFovRadians,
        static_cast<float>(Extent.Width) / Extent.Height, Out.NearPlane, Out.FarPlane);
    Out.ViewProjection = Out.Projection * Out.View;
    return Out.IsValid();
}

// All native and scene owners move logically to the terminal callback once
// RequestExit starts. The event thread keeps servicing only Window/Session.
class FLabRunOwner final
{
public:
    struct FSlot
    {
        Core::uint64 Token = 0;
        Renderer::FOutputTransformPreviewTicket Ticket;
        RHI::FRHIBorrowedAcquiredTarget Target;
        bool bAcquireAttempted = false;
        bool bSubmitted = false;
        bool bRenderCounted = false;
        bool bPresentQueued = false;
        bool bCancellationAcknowledged = false;
    };
    struct FPresentation
    {
        RHI::FRHIPresentationLease Lease;
        Core::uint32 Slot = 0;
    };

    explicit FLabRunOwner(Core::TSharedPtr<IDemoBackendRuntime> InBackend)
        : Backend(std::move(InBackend)), Frames(Core::MakeShared<FLabProductionFrameContext>())
    {
        Presentations.reserve(16);
    }

    void Fail(const Core::FString& Reason)
    {
        if (FirstFailure.IsEmpty())
        {
            FirstFailure = Reason.IsEmpty() ? "interactive lab operation failed" : Reason;
            std::cerr << "InteractiveLab first failure: " << FirstFailure.CStr() << std::endl;
        }
    }

    void FailOperation(const char* Operation, ERHIResult Result, const Core::FString& Reason)
    {
        Fail(std::string(Operation) + ": result=" + std::to_string(static_cast<int>(Result)) +
            "; " + Reason.ToStdString());
    }

    static Core::uint64 CaptureNow()
    { return static_cast<Core::uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count()); }
    Core::FString RequestCapture(Application::FInteractiveLabSession& Session,const Core::FString& Name,bool IncludeUI,bool Numeric)
    {
        if (!bSceneReady || ModeTransaction || bCapabilityRecovery || bNeedsResize || !FirstFailure.IsEmpty() ||
            CaptureStore.ExportRoot.IsEmpty() || !Session.GetEffectiveSettings()) return "Capture unavailable until output is stable.";
        const auto Text=Name.View();
        if (Text.empty() || Text.size()>80 || !std::all_of(Text.begin(),Text.end(),[](char C) {
            return (C>='a' && C<='z') || (C>='A' && C<='Z') || (C>='0' && C<='9') || C=='-' || C=='_';
        })) return "Use 1-80 letters, digits, hyphens or underscores for the capture name.";
        auto Slot=std::find_if(CaptureExports.begin(),CaptureExports.end(),[](const auto& E) { return !E.Id; });
        if (Slot==CaptureExports.end()) return "Capture queue busy (two requests).";
        if (NextCapture==std::numeric_limits<Core::uint64>::max()) return "Capture identity exhausted.";
        const auto& Effective=*Session.GetEffectiveSettings();
        FLabCaptureRequest R; R.RequestId=NextCapture++;
        auto& I=R.Target;
        I.SettingsGeneration=Effective.SettingsRevision;
        I.DisplayGeneration=Session.GetDisplayState().DisplayGeneration;
        I.OutputGeneration=Status.ResolvedState.ModeGeneration;
        I.Width=CurrentExtent.Width; I.Height=CurrentExtent.Height;
        I.OutputProfile=OutputResolved.OutputDeviceProfileId;
        I.Stage=Numeric ? Effective.DebugBypass.StageName : Core::FString("FinalOutput");
        if (I.Stage.IsEmpty()) return "Select a numeric diagnostic stage first.";
        I.Format=Numeric ? RHI::ERHIFormat::R16G16B16A16_Float : Status.ResolvedState.Format;
        I.bIncludeUI=IncludeUI && !Numeric;
        if (I.bIncludeUI && !Session.IsUIEnabled()) return "Enable UI before requesting a UI-inclusive capture.";
        I.Purpose=Numeric || OutputResolved.DynamicRange==Renderer::EOutputDynamicRange::HDR
            ? ELabCapturePurpose::HDRNumeric : ELabCapturePurpose::SDRPreview;
        const auto Result=Frames->RequestCapture(R,CaptureNow());
        if (Result!=ELabCaptureStatus::Pending) return "Capture request rejected or busy.";
        Slot->Id=R.RequestId; Slot->Stem=Core::FString(Name.ToStdString()+"-"+std::to_string(R.RequestId));
        CaptureStatus="Capture queued."; return CaptureStatus;
    }
    void ConsumeCaptures(bool Stop=false)
    {
        if (CaptureTask.IsActive())
        {
            Core::FString Result;
            if (!CaptureTask.Poll(Result)) return;
            CaptureStatus=std::move(Result);
            CaptureStaging.reset();
        }
        if (Stop) for (const auto& E : CaptureExports) if (E.Id) (void)Frames->CancelCapture(E.Id);
        FLabCaptureCompletion Completion;
        Core::FString Failure;
        const auto Done=Frames->ProcessCapture(++CaptureServiceFrame,CaptureNow(),[&](const auto& C,const auto& Buffer,const auto& Region) {
            const auto Export=std::find_if(CaptureExports.begin(),CaptureExports.end(),[&](const auto& E) { return E.Id==C.Request.RequestId; });
            Core::uint64 Bytes=0; Core::TArray<Core::uint8> Data;
            if (Stop || Export==CaptureExports.end() ||
                !RHI::TryGetRHITextureBufferCopyByteSize(Region,C.Request.Target.Format,Bytes) ||
                ReadProductionBuffer(CaptureBackend,Backend->GetDevice(),Buffer,Bytes,Data)!=ERHIResult::Success)
            { Failure="Capture readback failed."; return false; }
            const auto Store=CaptureStore;
            const auto Stem=Export->Stem;
            if (!CaptureTask.Start([C,Data=std::move(Data),Store,Stem]() -> Core::FString {
            FLabCaptureEncoded Encoded;
            if (!EncodeLabCapture(C,Data,STONER_LAB_STRINGIFY(STONER_DEMO_SOFTWARE_REVISION),Encoded))
            { return "Capture encoding rejected."; }
            auto PayloadStore=Store;
            if (!Encoded.bPNG)
            {
                Core::FString BuildRoot;
                if (!Core::FPlatformFileSystem::CreateDirectory("Build/InteractiveLab/Raw") ||
                    !Core::FPlatformFileSystem::CanonicalizeExistingPath("Build",BuildRoot).IsSuccess() ||
                    !Core::FPlatformFileSystem::CanonicalizeExistingPath("Build/InteractiveLab/Raw",PayloadStore.ExportRoot).IsSuccess())
                { return "Cannot prepare ignored raw capture storage."; }
                bool Inside=false;
                if (!Core::FPlatformFileSystem::CheckContainedPath(BuildRoot,PayloadStore.ExportRoot,Inside).IsSuccess() || !Inside)
                { return "Raw capture storage escaped Build."; }
            }
            const auto Payload=Application::ExportLabFile(PayloadStore,Core::FString(Stem.ToStdString()+(Encoded.bPNG ? ".png" : ".raw")),Encoded.Payload);
            if (!Payload.bPublished || !Payload.Status.IsSuccess())
            { return Payload.bPublished ? "Capture payload published; durability confirmation failed." :
                Payload.bTemporaryRetained ? "Capture export failed; temporary cleanup needs attention." :
                "Capture payload export rejected; choose a new name."; }
            const auto Report=Application::ExportLabFile(Store,Core::FString(Stem.ToStdString()+".json"),Encoded.Report);
            if (!Report.bPublished || !Report.Status.IsSuccess())
            { return Report.bPublished ? "Capture payload and report published; durability confirmation failed." :
                Report.bTemporaryRetained ? "Capture payload published; report temporary cleanup needs attention." :
                "Capture payload published, but report publication failed; choose a new name."; }
            return Core::FString("Capture exported: "+Report.TargetPath.ToStdString());
            })) { Failure="Cannot start capture export worker."; return false; }
            CaptureStaging=Buffer;
            CaptureStatus="Capture encoding/export in progress.";
            return true;
        },Completion);
        if (Done)
        {
            for (auto& E : CaptureExports) if (E.Id==Completion.Request.RequestId) E={};
            if (Completion.Status!=ELabCaptureStatus::Success)
            {
                if (!Failure.IsEmpty()) CaptureStatus=Failure;
                else switch (Completion.Status)
                {
                case ELabCaptureStatus::InvalidRequest: CaptureStatus="Capture unavailable: the selected image or output cannot be copied."; break;
                case ELabCaptureStatus::GenerationMismatch: CaptureStatus="Capture cancelled: settings, output or UI changed before recording."; break;
                case ELabCaptureStatus::TimedOut: CaptureStatus="Capture timed out; GPU resources have now retired."; break;
                case ELabCaptureStatus::AllocationFailed: CaptureStatus="Capture staging allocation failed."; break;
                case ELabCaptureStatus::DeviceLost: CaptureStatus="Capture cancelled because the graphics device stopped."; break;
                case ELabCaptureStatus::Cancelled: CaptureStatus="Capture cancelled."; break;
                default: CaptureStatus="Capture readback or export failed."; break;
                }
            }
        }
    }

    bool Load(const FDemoConfiguration& Config, FWindowExtent Extent)
    {
        if (!Backend->GetDevice() || !Backend->GetDevice()->GetCapabilities().bSupportsDeferredSubmission)
        { Fail("native backend does not support deferred preview submission"); return false; }
        Core::TArray<Core::uint8> Bytes;
        Asset::FAssetTargetProfileEvidence Evidence;
        Asset::FAssetDigest Generation;
        if (!Core::FPlatformFileSystem::ReadFile(Config.TargetProfilePath, Bytes) ||
            Asset::FAssetCookContractCodec::ParseTargetProfile(Bytes, Evidence) != Asset::EAssetResult::Success ||
            Asset::FAssetDigest::ParseLowerHex(Config.StrictGeneration, Generation) != Asset::EAssetResult::Success)
        { Fail("invalid strict cooked generation or target profile"); return false; }
        FProductionContentSessionConfig LoadConfig;
        LoadConfig.PublicationRoot = Config.CookedPublicationRoot;
        LoadConfig.LeaseCoordinationRoot = Config.LeaseCoordinationRoot;
        LoadConfig.RootAssetIdentity = Config.ProductionRoot;
        LoadConfig.bCollectSourceIdentity = true;
        LoadConfig.ExpectedGeneration = Generation;
        LoadConfig.TargetEvidence = Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(Evidence);
        Assets = Core::MakeShared<FProductionContentSession>();
        if (Assets->Load(LoadConfig, Closure) != Asset::EAssetResult::Success)
        { Fail(Assets->Inspect().FirstFailure); return false; }
        Renderer::FStaticModelRealizationRequest Realize;
        Realize.Device = Backend->GetDevice();
        Realize.Model = Closure.Model;
        Realize.Dependencies = Closure.Dependencies;
        Realize.TargetEvidence = LoadConfig.TargetEvidence;
        if (Realize.Dependencies.Textures.empty())
        { Fail("strict scene closure has no cooked textures"); return false; }
        for (const auto& Texture : Realize.Dependencies.Textures)
            Realize.TextureTargetProfiles.push_back({Texture->GetId(),
                Renderer::FTextureTargetProfile::DesktopDefault(Texture->GetInfo())});
        Realize.RenderTargets.SampleCount = RHI::ERHISampleCount::One;
        Realize.RenderTargets.ColorFormats = {RHI::ERHIFormat::R8G8B8A8_UNorm,
            RHI::ERHIFormat::R16G16B16A16_Float, RHI::ERHIFormat::R16G16B16A16_Float};
        Realize.RenderTargets.DepthStencilFormat = RHI::ERHIFormat::D32_Float;
        Renderer::FStaticModelRealizationInspection Inspection;
        if (Renderer::FStaticModelRealizer::Realize(Realize, Scene, Inspection) != ERHIResult::Success)
        { Fail(Inspection.FirstFailure.Reason); return false; }
        FProductionContentCompositionConfig CompositionConfig;
        CompositionConfig.WorkloadRevision = Config.WorkloadRevision;
        CompositionConfig.Width = Extent.Width; CompositionConfig.Height = Extent.Height;
        Core::FString Reason;
        if (!FProductionContentCompositionBuilder::Build(Scene, CompositionConfig, Composition, &Reason))
        { Fail(Reason); return false; }
        if (Backend->QueryLabPresentation(Status) != ERHIResult::Success ||
            !ResolveDemoOutputTransformSettings(Config, Status.Capabilities.NativeReferenceWhiteNits,
                OutputSettings, &OutputResolved, &Reason))
        { Fail(Reason.IsEmpty() ? Core::FString("lab presentation capability resolution failed") : Reason); return false; }
        OutputSettings.bRequireReadback = false;
        OutputSettings.bRequirePresentation = true;
        OutputResolved = Renderer::FOutputTransformSettingsValidator().Validate(OutputSettings).Settings;
        PresentationFormat = OutputResolved.OutputFormat;
        if (!Status.Capabilities.SupportsPair(PresentationFormat, OutputResolved.ColorSpace) &&
            OutputResolved.DynamicRange == Renderer::EOutputDynamicRange::SDR &&
            PresentationFormat == RHI::ERHIFormat::R8G8B8A8_UNorm &&
            Status.Capabilities.SupportsPair(RHI::ERHIFormat::B8G8R8A8_UNorm, OutputResolved.ColorSpace))
            PresentationFormat = RHI::ERHIFormat::B8G8R8A8_UNorm;
        auto Request = PresentationRequest(OutputResolved, Extent);
        Request.PreferredFormat = PresentationFormat;
        Request.SurfaceCapabilityGeneration = Status.Capabilities.CapabilityGeneration;
        if (Backend->PrepareLabPresentation(Request, Status, &Reason) != ERHIResult::Success)
        { Fail(Reason); return false; }
        FLabProductionFrameContextConfig FrameConfig;
        FrameConfig.Device = Backend->GetDevice(); FrameConfig.SceneLease = Scene;
        FrameConfig.Composition = Composition; FrameConfig.RenderShaders = Closure.RenderShaders;
        FrameConfig.RenderShaderPayloads = Closure.RenderShaderPayloads;
        FrameConfig.TargetEvidence = Evidence; FrameConfig.OutputSettings = OutputSettings;
        if (Frames->Initialize(FrameConfig, &Reason) != ERHIResult::Success)
        { Fail(Reason); return false; }
        TargetEvidence = Evidence;
        CurrentExtent = Extent;
        ObservedCapabilities = Status.Capabilities;
        bSceneReady = true;
        return true;
    }

    Application::EApplicationResult EnableUI(Core::uint64 SessionId)
    {
        FInteractiveLabShaders Candidate;
        Core::FString Reason;
        if (!bSceneReady || PrepareInteractiveLabShaders(Closure,Closure.GenerationIdentity,
                TargetEvidence,Candidate,Reason) != Asset::EAssetResult::Success)
        {
            std::cerr << "InteractiveLab UI unavailable: " << Reason.CStr() << std::endl;
            return Application::EApplicationResult::RuntimeUnavailable;
        }
        if (!UI) UI = Core::MakeShared<Renderer::FUIRenderSession>(Backend->GetDevice(),SessionId);
        UIShaders = std::move(Candidate);
        return Application::EApplicationResult::Success;
    }
    bool ConfigureSessionSettings(Application::FInteractiveLabSession& Session)
    {
        Application::FLabSettingsSnapshot Initial;
        Initial.CameraRevision = Session.GetCameraState().CameraRevision;
        Initial.SettingsRevision = Initial.OutputModeGeneration = 1;
        Initial.DisplayGeneration = Session.GetDisplayState().DisplayGeneration;
        Initial.RequestedProfileId = Initial.EffectiveProfileId = OutputResolved.OutputDeviceProfileId;
        Initial.SdrToneMapVersion = OutputSettings.SDRToneMapVersion.IsEmpty() ? Core::FString(Renderer::GDefaultSDRToneMapVersion) : OutputSettings.SDRToneMapVersion;
        Initial.HdrViewingVersion = OutputSettings.HDRViewingVersion.IsEmpty() ? Core::FString(Renderer::GInitialHDRViewingVersion) : OutputSettings.HDRViewingVersion;
        Initial.ExposureStops = OutputResolved.ManualExposureStops; Initial.bUIVisible = Session.IsUIEnabled();
        Initial.UIReferenceWhiteNits = OutputResolved.ReferenceWhiteNits;
        Initial.NativePackingWhiteNits = Status.ResolvedState.ReferenceWhiteNits;
        const auto Caps = OutputCapabilities(Initial.DisplayGeneration);
        return Session.ConfigureSettings(Initial,Caps);
    }

    bool ResolveOutput(const Application::FLabSettingsSnapshot& Settings,
        Renderer::FOutputTransformSettings& Candidate,
        Renderer::FResolvedOutputTransformSettings& Resolved,
        RHI::ERHIFormat& Format, Core::FString& Reason) const
    {
        FDemoConfiguration Config;
        Config.GraphicsBackend = Backend->GetBackend();
        Config.OutputDeviceProfileId = Settings.EffectiveProfileId;
        Config.OutputExposureStops = Settings.ExposureStops;
        Config.OutputTransformVersion = Settings.EffectiveProfileId.View().starts_with("Hdr.")
            ? Settings.HdrViewingVersion : Settings.SdrToneMapVersion;
        if (!ResolveDemoOutputTransformSettings(Config,
                Status.Capabilities.NativeReferenceWhiteNits, Candidate, &Resolved, &Reason)) return false;
        Candidate.bRequireReadback = false;
        Candidate.DiagnosticBypass.StageName=Settings.DebugBypass.StageName;
        Candidate.DiagnosticBypass.Mode=Settings.DebugBypass.Mode;
        Candidate.DiagnosticBypass.VisualizationMinimum=Settings.DebugBypass.VisualizationMinimum;
        Candidate.DiagnosticBypass.VisualizationMaximum=Settings.DebugBypass.VisualizationMaximum;
        if (!FProductionContentDeferredExecutionBuilder::ValidatePreviewOutputSettings(Composition,Candidate))
        { Reason="Diagnostic selection is unavailable for this output"; return false; }
        const auto Validation = Renderer::FOutputTransformSettingsValidator().Validate(Candidate);
        if (!Validation.Succeeded()) { Reason = Validation.Diagnostics.Dump(); return false; }
        Resolved = Validation.Settings;
        Format = Resolved.OutputFormat;
        if (!Status.Capabilities.SupportsPair(Format,Resolved.ColorSpace) &&
            Resolved.DynamicRange == Renderer::EOutputDynamicRange::SDR &&
            Format == RHI::ERHIFormat::R8G8B8A8_UNorm &&
            Status.Capabilities.SupportsPair(RHI::ERHIFormat::B8G8R8A8_UNorm,Resolved.ColorSpace))
            Format = RHI::ERHIFormat::B8G8R8A8_UNorm;
        if (!Status.Capabilities.SupportsPair(Format,Resolved.ColorSpace) ||
            (Resolved.NativeEncoding == RHI::ERHIPresentationNativeEncoding::MetalEdr &&
             !Status.Capabilities.bSupportsExtendedRange))
        { Reason = "Output profile is unavailable on the current display"; return false; }
        return true;
    }

    Application::FLabSettingsCapabilities OutputCapabilities(Core::uint64 Generation) const
    {
        Application::FLabSettingsCapabilities Caps;
        Caps.DisplayGeneration = Generation;
        Caps.DebugStages.push_back({"SceneColorHandoff",Renderer::ERenderGraphColorDomain::SceneLinearRec709D65,{}});
        Caps.DebugStages.push_back({"ManualExposure",Renderer::ERenderGraphColorDomain::SceneLinearRec709D65,{}});
        for (const auto& Profile : Renderer::FOutputTransformSettingsValidator().GetProfiles())
        {
            Application::FLabSettingsSnapshot Settings;
            Settings.EffectiveProfileId = Profile.ProfileId;
            Settings.SdrToneMapVersion = Renderer::GDefaultSDRToneMapVersion;
            Settings.HdrViewingVersion = Renderer::GInitialHDRViewingVersion;
            Renderer::FOutputTransformSettings Candidate;
            Renderer::FResolvedOutputTransformSettings Resolved;
            RHI::ERHIFormat Format;
            Core::FString Reason;
            if (ResolveOutput(Settings,Candidate,Resolved,Format,Reason))
            {
                Caps.Outputs.push_back({Profile.ProfileId,Resolved.ReferenceWhiteNits,Resolved.ReferenceWhiteNits});
                Caps.DebugStages.push_back({Resolved.DynamicRange==Renderer::EOutputDynamicRange::SDR
                    ? "SDRToneMap" : "HDRViewingTransform",Resolved.DisplayLinearDomain,Profile.ProfileId});
            }
        }
        return Caps;
    }

    bool OutputCapabilitiesChanged() const
    {
        const auto& Caps = Status.Capabilities;
        return Caps.SupportedPairs != ObservedCapabilities.SupportedPairs ||
            Caps.NativeReferenceWhiteNits != ObservedCapabilities.NativeReferenceWhiteNits ||
            Caps.bSupportsExtendedRange != ObservedCapabilities.bSupportsExtendedRange;
    }

    bool ObserveOutputCapabilities(Application::FWindow& Window)
    {
        if (!bSceneReady || Backend->QueryLabPresentation(Status) != ERHIResult::Success) return false;
        const auto& Caps = Status.Capabilities;
        const bool Changed = OutputCapabilitiesChanged();
        ObservedCapabilities = Caps;
        if (Changed) Window.QueueEvent(Application::FWindowEvent::DisplayCapabilitiesChanged());
        return Changed;
    }

    void ApplySessionSettings(Application::FInteractiveLabSession& Session, Core::uint32 Budget)
    {
        const auto* Effective = Session.GetEffectiveSettings();
        if (!Effective || !bSceneReady || !FirstFailure.IsEmpty()) return;
        const auto& Display = Session.GetDisplayState();
        // A stale operation must relinquish its token before the session can
        // process the newer resize. No stale native result may become effective.
        if (ModeTransaction && ModeTransaction->Settings.DisplayGeneration != Display.DisplayGeneration)
        {
            (void)Session.CompleteSettingsTransaction(ModeTransaction->Token,false,false);
            ModeTransaction.reset();
            return;
        }
        if (Display.bMinimized || !Display.DrawableExtent.IsPositive()) return;
        Core::FString Reason;
        if (Backend->QueryLabPresentation(Status) != ERHIResult::Success)
        { Fail("lab settings capability query failed"); return; }
        // Capabilities may change after event polling but before admission.
        // Hold new frames until the next observation tags that change in the
        // window/session generation and the lifecycle drain acknowledges it.
        if (OutputCapabilitiesChanged()) { bCapabilityRecovery = true; return; }
        if (!ModeTransaction)
        {
            if (!CanPrepareUI()) return;
            if (Session.GetRequestedSettings()->DisplayGeneration != Display.DisplayGeneration)
                (void)Session.RefreshSettingsCapabilities(OutputCapabilities(Display.DisplayGeneration),Status.bPrepared);
            const auto* Transaction = Session.BeginSettingsTransaction(true);
            if (!Transaction) return;
            Renderer::FOutputTransformSettings Candidate;
            Renderer::FResolvedOutputTransformSettings Resolved;
            RHI::ERHIFormat Format;
            if (!ResolveOutput(Transaction->Settings,Candidate,Resolved,Format,Reason))
            {
                (void)Session.CompleteSettingsTransaction(Transaction->Token,false,Status.bPrepared);
                return;
            }
            const auto Updated = Frames->UpdateOutputSettings(Candidate,&Reason);
            if (Updated == ERHIResult::Success && Status.bPrepared &&
                !bCapabilityRecovery && CurrentExtent == Display.DrawableExtent)
            {
                if (Session.CompleteSettingsTransaction(Transaction->Token,true,true))
                { OutputSettings = std::move(Candidate); OutputResolved = Resolved; }
                return;
            }
            if (Updated != ERHIResult::ResizeRequired && Updated != ERHIResult::Success)
            { (void)Session.CompleteSettingsTransaction(Transaction->Token,false,Status.bPrepared); return; }
            ModeTransaction = *Transaction;
            ModeFormerGeneration = Status.ResolvedState.SwapchainImageGeneration;
        }

        // Acquire-history retirement needs continued acquisitions from the
        // current generation. Keep that output until its predecessor retires;
        // the native backend still owns the two-generation admission limit.
        if (Status.RuntimeSnapshot.NativePresentation.RetiringGeneration != 0 && Status.bPrepared &&
            !bNeedsResize && CurrentExtent == Display.DrawableExtent && !Session.IsSettingsPaused())
        {
            Progress(false);
            Admit(Session,Budget);
            return;
        }
        Progress(true);
        if (BusySlots() != 0 || !FirstFailure.IsEmpty()) return;
        Renderer::FOutputTransformSettings Candidate;
        Renderer::FResolvedOutputTransformSettings Resolved;
        RHI::ERHIFormat Format;
        if (!ResolveOutput(ModeTransaction->Settings,Candidate,Resolved,Format,Reason) ||
            Resolved.ReferenceWhiteNits != ModeTransaction->Settings.NativePackingWhiteNits)
        {
            (void)Session.CompleteSettingsTransaction(ModeTransaction->Token,false,Status.bPrepared);
            ModeTransaction.reset();
            return;
        }
        auto Change = PresentationRequest(Resolved,Display.DrawableExtent);
        Change.PreferredFormat = Format;
        Change.SurfaceCapabilityGeneration = Status.Capabilities.CapabilityGeneration;
        const auto Result = Backend->ReconfigureLabPresentation(Change,Status,&Reason);
        if (Result == ERHIResult::NotReady || Result == ERHIResult::Timeout) return;
        const auto Token = ModeTransaction->Token;
        ModeTransaction.reset();
        if (Result != ERHIResult::Success)
        {
            // A usable replacement is not proof that our former bindings can
            // resume. Failure after native replacement must leave them paused.
            const bool FormerUsable = !Session.IsSettingsPaused() && Status.bPrepared && ModeFormerGeneration != 0 &&
                Status.ResolvedState.SwapchainImageGeneration == ModeFormerGeneration;
            (void)Session.CompleteSettingsTransaction(Token,false,FormerUsable);
            return;
        }
        // Native replacement has succeeded: even a later CPU/frame failure
        // cannot make the retired previous swapchain usable again.
        OutputSettings = std::move(Candidate); OutputResolved = Resolved;
        PresentationFormat = Format; CurrentExtent = Display.DrawableExtent;
        const auto ExtentResult = Frames->Reconfigure(Change.Width,Change.Height,&Reason);
        const auto FrameResult = ExtentResult == ERHIResult::Success
            ? Frames->ReconfigureOutputSettings(OutputSettings,&Reason) : ExtentResult;
        if (FrameResult != ERHIResult::Success)
        {
            (void)Session.CompleteSettingsTransaction(Token,false,false);
            FailOperation("output frame reconfiguration",FrameResult,Reason);
            return;
        }
        bNeedsResize = false;
        bCapabilityRecovery = false;
        (void)Session.CompleteSettingsTransaction(Token,true,true);
    }

    bool CanPrepareUI() const noexcept
    {
        return bSceneReady && !bNeedsResize && FirstFailure.IsEmpty() &&
            std::any_of(Slots.begin(),Slots.end(),[](const auto& Slot) { return !Slot.Ticket.IsValid(); });
    }

    void Progress(bool bStop, bool bPaused = false)
    {
        // Poll leases independently; a pending presentation never keeps a
        // completed frame's mutable uniforms or attachments busy.
        for (Core::usize Index = 0; Index < Presentations.size();)
        {
            bool Complete = false;
            Core::FString Reason;
            auto& Record = Presentations[Index];
            const auto Result = Backend->PollLabPresentation(Record.Lease, Complete, &Reason);
            if (Complete)
            {
                bool Retired = false;
                const auto RetireResult = Frames->PollPresentation(Record.Lease.Frame.FrameToken, Record.Slot, Retired, &Reason);
                if (RetireResult != ERHIResult::Success || !Retired)
                { FailOperation("retire-presentation",RetireResult,Reason); ++Index; }
                else Presentations.erase(Presentations.begin() + Index);
            }
            else
            {
                if (Result != ERHIResult::Success && !Pending(Result)) FailOperation("poll-presentation",Result,Reason);
                ++Index;
            }
        }
        for (Core::uint32 Index = 0; Index < Slots.size(); ++Index)
        {
            auto& Slot = Slots[Index];
            if (!Slot.Token) continue;
            if (!Slot.Ticket.IsValid())
            {
                if (Frames->GetFrameState(Slot.Token, Index) == ELabProductionFrameState::Free)
                { Slot = {}; continue; }
                if (bStop || !FirstFailure.IsEmpty()) CancelWithoutTicket(Index);
                continue;
            }
            Renderer::FOutputTransformExecutor Executor;
            if (Slot.bSubmitted)
            {
                const auto Poll = Executor.PollPreview(Slot.Ticket);
                if (!Poll.bRenderCompleted)
                {
                    if (Poll.NativeResult != ERHIResult::Success && !Pending(Poll.NativeResult))
                        Fail("lab render completion failed");
                    continue;
                }
                if (!Slot.bRenderCounted) { Slot.bRenderCounted = true; ++Completed; }
                if (Poll.Result != Renderer::EOutputTransformResult::Success) Fail("lab render completed with failure");
                if (bPaused) continue;
                if (!bStop && FirstFailure.IsEmpty() && !Slot.bPresentQueued)
                {
                    if (Presentations.size() >= 16) continue;
                    RHI::FRHIRenderLease Render;
                    Core::FString Reason;
                    if (!Frames->GetRenderLease(Slot.Token, Index, Render))
                    { Fail("completed lab frame has no exact render lease"); continue; }
                    RHI::FRHIPresentationLease Lease;
                    const auto PresentResult = Backend->PresentLabTarget(Slot.Target, Render, Lease, &Reason);
                    if (Lease.IsValid())
                    {
                        // Ownership admission and operation failure are separate.
                        // Keep a published lease even when present reports error.
                        Presentations.push_back({Lease, Index});
                        Slot.bPresentQueued = true;
                        ++Presented;
                        if (Frames->QueuePresentation(Slot.Token, Index, Lease, &Reason) != ERHIResult::Success)
                            Fail(Reason);
                    }
                    if (PresentResult == ERHIResult::ResizeRequired) bNeedsResize = true;
                    else if (PresentResult != ERHIResult::Success && !Pending(PresentResult)) FailOperation("present", PresentResult, Reason);
                    if (!Slot.bPresentQueued && FirstFailure.IsEmpty()) continue;
                }
            }
            if (bPaused) continue;
            const auto Retired = Executor.RetirePreview(Slot.Ticket);
            if (Retired.bRetired)
            {
                if (Slot.bSubmitted && !Slot.bPresentQueued) ++CancelledSubmissions;
                Slot = {};
            }
            else if (Retired.NativeResult != ERHIResult::Success && !Pending(Retired.NativeResult))
                Fail("lab preview retirement failed");
        }
    }

    void Admit(Application::FInteractiveLabSession& Session, Core::uint32 Budget)
    {
        const auto& Camera = Session.GetCameraState();
        if (!bSceneReady || Session.IsSettingsPaused() || bNeedsResize || !FirstFailure.IsEmpty() || (Budget && Submitted - CancelledSubmissions >= Budget)) return;
        std::array<Core::uint32,2> AdmissionOrder{0,1};
        const auto Priority = [&](Core::uint32 Index) {
            return Slots[Index].Token ? Slots[Index].Token : std::numeric_limits<Core::uint64>::max();
        };
        if (Priority(1)<Priority(0)) std::swap(AdmissionOrder[0],AdmissionOrder[1]);
        for (const auto Index : AdmissionOrder)
        {
            auto& Slot = Slots[Index];
            if (Slot.Ticket.IsValid()) continue;
            if (!Slot.Token)
            {
                Core::uint32 ReservedCount = 0;
                for (const auto& PendingSlot : Slots)
                    ReservedCount += PendingSlot.Token != 0 && !PendingSlot.bSubmitted ? 1 : 0;
                if (Budget && Submitted - CancelledSubmissions + ReservedCount >= Budget) continue;
                if (Presentations.size() >= 16) return;
                if (NextToken == std::numeric_limits<Core::uint64>::max())
                { Fail("lab frame identity exhausted"); return; }
                Slot.Token = NextToken++;
                Core::FString Reason;
                const auto Reserved = Frames->ReserveFrame(Slot.Token, Index, &Reason);
                if (Reserved != ERHIResult::Success)
                {
                    Slot = {};
                    if (!Pending(Reserved)) FailOperation("reserve-frame",Reserved,Reason);
                    return;
                }
            }
            Core::FString Reason;
            const auto Acquired = Backend->AcquireLabTarget(Slot.Token, Index, Slot.Target, &Reason);
            Slot.bAcquireAttempted = Backend->OwnsLabAcquireAttempt(Slot.Token, Index);
            if (Acquired == ERHIResult::ResizeRequired) { bNeedsResize = true; return; }
            // Finish older acquisition before preparing a newer frame. Do not
            // let delayed slot tokens reach the UI validator out of order.
            // Presentation/retirement remains free to progress on the next poll.
            if (Pending(Acquired)) return;
            if (Acquired != ERHIResult::Success) { FailOperation("acquire", Acquired, Reason); return; }
            const auto Begun = Frames->BeginFrame(Slot.Token, Index, Slot.Target, &Reason);
            if (Begun != ERHIResult::Success)
            { FailOperation("begin-frame", Begun, Reason); return; }
            auto Frame = Composition;
            Frame.FrameToken = Slot.Token;
            Frame.CameraPosition = Camera.Position;
            auto& View = Frame.DeferredInputs.View;
            View.View = Camera.View; View.Projection = Camera.Projection;
            View.ViewProjection = Camera.ViewProjection;
            FProductionCameraPreset FrameCamera;
            if (!BuildProductionCameraPreset(Composition.WorkloadRevision,
                Camera.View, Camera.Projection, FrameCamera, &Reason))
            { Fail(std::string("frame-camera: ")+Reason.ToStdString()); return; }
            View.InverseViewProjection = FrameCamera.InverseViewProjection;
            View.CameraPosition = Camera.Position;
            View.Extent = {CurrentExtent.Width, CurrentExtent.Height};
            Frame.DeferredInputs.Output.Extent = View.Extent;
            const auto RetainedBackend = Backend;
            FLabCaptureRequest PendingCapture;
            const bool HasCapture=Frames->GetPendingCapture(PendingCapture);
            FLabCaptureFrame CaptureFrame;
            if (HasCapture)
            {
                CaptureFrame={Slot.Token,PendingCapture.Target,Renderer::EFrameExecutionPurpose::InteractivePreview,true};
                CaptureFrame.Identity.SettingsGeneration=Session.GetEffectiveSettings()->SettingsRevision;
                CaptureFrame.Identity.DisplayGeneration=Session.GetDisplayState().DisplayGeneration;
                CaptureFrame.Identity.OutputGeneration=Status.ResolvedState.ModeGeneration;
                CaptureFrame.Identity.OutputProfile=OutputResolved.OutputDeviceProfileId;
                CaptureFrame.Identity.Width=CurrentExtent.Width; CaptureFrame.Identity.Height=CurrentExtent.Height;
            }
            FLabProductionFrameContext::FPrepareUI PrepareUI;
            if (Session.IsUIEnabled() && UI && (!HasCapture || PendingCapture.Target.bIncludeUI))
            {
                PrepareUI = [&, Token=Slot.Token](const auto& Resources,Core::uint64 Available,
                    Core::TSharedPtr<Renderer::FUIRenderFrame>& OutFrame,
                    Core::TSharedPtr<FProductionContentPreviewGraph>& OutGraph) {
                    const auto& SceneInput = Resources.Bindings.OutputTransformStages.back().Input;
                    const auto& Display = Session.GetDisplayState();
                    const auto Required = static_cast<Core::uint64>(Display.DrawableExtent.Width) *
                        Display.DrawableExtent.Height * 8ULL;
                    if (Required > Available) return ERHIResult::Unavailable;
                    const auto* Effective = Session.GetEffectiveSettings();
                    const auto Revision = Effective ? Effective->SettingsRevision : 1;
                    Renderer::FUIDrawSnapshot Snapshot(Session.GetSessionId(),Token,Revision,Display.DisplayGeneration);
                    if (Session.ExtractUIDrawSnapshot(Snapshot) != ERHIResult::Success)
                        return ERHIResult::NotReady;
                    Renderer::FUICompositionSettings Settings;
                    Settings.UIWhiteMultiplier = Effective ? Effective->UIWhiteMultiplier : 1.0f;
                    Settings.OutputProfileId = OutputResolved.OutputDeviceProfileId;
                    Settings.BlendDomain = OutputResolved.DisplayLinearDomain;
                    Settings.UIReferenceWhiteNits = OutputResolved.ReferenceWhiteNits;
                    Settings.NativePackingWhiteNits = Status.ResolvedState.ReferenceWhiteNits;
                    Settings.DisplayGeneration = Display.DisplayGeneration;
                    Core::TSharedPtr<FProductionContentPreviewGraph> Graph;
                    Renderer::FUIDiagnosticRenderInput Diagnostic;
                    const bool HasWidget=std::any_of(Snapshot.GetCommands().begin(),Snapshot.GetCommands().end(),
                        [](const auto& Command) { return Command.bDiagnosticWidget; });
                    Settings.bDiagnosticWidgetVisible=HasWidget;
                    if (HasWidget)
                    {
                        if (!FProductionContentDeferredExecutionBuilder::BuildPreviewGraph(Frame,
                            Resources.OutputSettings,&Settings,Resources.Bindings.FormalOutput->GetFormat(),Graph) ||
                            !Graph->Plan.HasDiagnosticWidget()) return ERHIResult::Unavailable;
                        Diagnostic.Selection=Graph->Plan.DiagnosticBypass;
                        if (Diagnostic.Selection.SourceStageName==Core::FString("SceneColorHandoff"))
                            Diagnostic.Source=Resources.Bindings.FinalOutput;
                        else if (Resources.Bindings.OutputTransformStages.size()==3)
                            for (const auto& Stage : Resources.OutputTransformPlan.Stages)
                            {
                                if (Stage.StageId!=Diagnostic.Selection.SourceStageId ||
                                    Stage.Name!=Diagnostic.Selection.SourceStageName ||
                                    Stage.OutputDomain!=Diagnostic.Selection.SourceDomain) continue;
                                // This realization owns exactly exposure, tone/viewing and
                                // output-transfer stages. Their log labels are not graph IDs.
                                if (Stage.Kind==Renderer::EOutputTransformStageKind::ManualExposure)
                                    Diagnostic.Source=Resources.Bindings.OutputTransformStages[0].Output;
                                else if (Stage.Kind==Renderer::EOutputTransformStageKind::SDRToneMap ||
                                    Stage.Kind==Renderer::EOutputTransformStageKind::HDRViewingTransform)
                                    Diagnostic.Source=Resources.Bindings.OutputTransformStages[1].Output;
                            }
                        if (!Diagnostic.Source) return ERHIResult::Unavailable;
                        Diagnostic.Graph=Core::TSharedPtr<const Renderer::FRenderGraph>(Graph,&Graph->Graph);
                        Diagnostic.Resource=Graph->Declaration.DiagnosticOutput;
                        Diagnostic.Producer=Graph->Declaration.DiagnosticVisualizationPass;
                        Diagnostic.Consumer=Graph->Declaration.UIPass;
                        Diagnostic.Shaders=UIShaders.Diagnostic.ModuleDescriptions;
                        Diagnostic.RemainingAttachmentBytes=Available-Required;
                    }
                    const auto Prepared = UI->PrepareFrame(Snapshot,Settings,Revision,0,SceneInput,
                        UIShaders.Draw.ModuleDescriptions,UIShaders.Copy.ModuleDescriptions,OutFrame,HasWidget ? &Diagnostic : nullptr);
                    if (Prepared==ERHIResult::Success) OutGraph=std::move(Graph);
                    return Prepared == ERHIResult::InvalidState || Prepared == ERHIResult::Unsupported
                        ? ERHIResult::Unavailable : Prepared;
                };
            }
            const auto Recorded = RecordLabProductionPreview(Frames, Frame, Index, Status.ResolvedState,
                [RetainedBackend](Core::uint64 Token, Core::uint32 SlotIndex,
                    const Core::TSharedPtr<RHI::IRHIFence>& Fence, bool& Acknowledged) {
                    return RetainedBackend->CancelLabTarget(Token, SlotIndex, Fence, Acknowledged);
                }, Slot.Ticket, PrepareUI, &Reason,HasCapture ? &CaptureFrame : nullptr,CaptureNow());
            if (Recorded.Result != Renderer::EOutputTransformResult::Success)
            {
                if (const auto* Error = Recorded.Diagnostics.GetFirstError()) Reason = Error->Message;
                FailOperation("lab preview graph recording", Recorded.NativeResult, Reason); return;
            }
            const auto Queued = Renderer::FOutputTransformExecutor().SubmitPreview(Slot.Ticket);
            Slot.bSubmitted = Queued.bQueued;
            if (Slot.bSubmitted)
            {
                ++Submitted;
                const auto* Resources = Frames->GetResources(Slot.Token,Index);
                if (Resources)
                {
                    LastRecordedExposureStops = Resources->OutputTransformPlan.ResolvedSettings.ManualExposureStops;
                    if (Resources->OutputTransformPlan.TerminalUI)
                    {
                        LastRecordedUIWhiteMultiplier = Resources->OutputTransformPlan.TerminalUI->UIWhiteMultiplier;
                        LastRecordedUIFrameToken = Slot.Token;
                        LastRecordedUISettings = *Resources->OutputTransformPlan.TerminalUI;
                        LastRecordedUIOutputTransferCount = static_cast<Core::uint32>(std::count_if(
                            Resources->OutputTransformPlan.Stages.begin(),Resources->OutputTransformPlan.Stages.end(),
                            [](const auto& Stage) { return Stage.Kind==Renderer::EOutputTransformStageKind::OutputDeviceTransform; }));
                    }
                    LastRecordedTransformVersion = Resources->OutputTransformPlan.ResolvedSettings.TransformStrategyVersion;
                    LastRecordedSettingsRevision = Session.GetEffectiveSettings() ? Session.GetEffectiveSettings()->SettingsRevision : 1;
                }
                if (Resources && Resources->OutputTransformPlan.TerminalUI) ++UIFramesSubmitted;
                else if (Session.IsUIEnabled() && (!HasCapture || PendingCapture.Target.bIncludeUI))
                {
                    ++UISceneFallbackFrames;
                    if (UISceneFallbackFrames <= 8)
                        std::cerr << "InteractiveLab UI fallback: frame=" << Slot.Token
                            << " preparation=" << static_cast<int>(Frames->Snapshot().LastUIPreparationResult)
                            << " attachments=" << Frames->Snapshot().ActiveAttachmentBytes
                            << " reason=" << Session.GetUIFailure().CStr() << '\n';
                }
                if (Resources && Resources->OutputTransformPlan.HasDiagnosticWidget()) ++DiagnosticFramesSubmitted;
            }
            if (!Queued.Accepted()) { Fail("lab preview submission failed"); return; }
            if (Budget && Submitted - CancelledSubmissions >= Budget) return;
        }
    }

    void CancelWithoutTicket(Core::uint32 Index)
    {
        auto& Slot = Slots[Index];
        Core::FString Reason;
        if (!Slot.bCancellationAcknowledged)
        {
            bool Ack = !Slot.bAcquireAttempted;
            const auto Result = Ack ? ERHIResult::Success :
                Backend->CancelLabTarget(Slot.Token, Index, nullptr, Ack, &Reason);
            if (!Ack || Result != ERHIResult::Success)
            {
                if (!Pending(Result) && Result != ERHIResult::Success) FailOperation("cancel-native-target",Result,Reason);
                return;
            }
            Slot.bCancellationAcknowledged = true;
        }
        if (Frames->GetFrameState(Slot.Token, Index) != ELabProductionFrameState::Cancelled)
        {
            const auto Cancelled = Frames->CancelFrame(Slot.Token, Index, &Reason);
            if (Cancelled != ERHIResult::Success)
            { FailOperation("cancel-frame",Cancelled,Reason); return; }
        }
        const auto Result = Frames->RetireCancelled(Slot.Token, Index, &Reason);
        if (Frames->GetFrameState(Slot.Token, Index) == ELabProductionFrameState::Free) Slot = {};
        if (Result != ERHIResult::Success && !Pending(Result)) FailOperation("retire-cancelled",Result,Reason);
    }

    Core::uint32 BusySlots() const noexcept
    {
        Core::uint32 Count = 0;
        for (const auto& Slot : Slots) Count += Slot.Token != 0 ? 1 : 0;
        return Count;
    }

    FInteractiveLabServiceResponse Service(const Application::FInteractiveLabServiceRequest& Request)
    {
        FInteractiveLabServiceResponse Out;
        Out.Status = EInteractiveLabServiceStatus::NotReady;
        Out.bAccepted = true;
        if (Request.Phase == EInteractiveLabServicePhase::Transition)
        {
            if (!bSceneReady) { Out.Status = EInteractiveLabServiceStatus::Invalid; return Out; }
            if (Backend->QueryLabPresentation(Status) != ERHIResult::Success)
            { Fail("lab transition capability query failed"); Out.Status = EInteractiveLabServiceStatus::Failed; return Out; }
            Application::FLabSettingsSnapshot Current;
            Current.EffectiveProfileId = OutputResolved.OutputDeviceProfileId;
            Current.SdrToneMapVersion = OutputSettings.SDRToneMapVersion.IsEmpty()
                ? Core::FString(Renderer::GDefaultSDRToneMapVersion) : OutputSettings.SDRToneMapVersion;
            Current.HdrViewingVersion = OutputSettings.HDRViewingVersion.IsEmpty()
                ? Core::FString(Renderer::GInitialHDRViewingVersion) : OutputSettings.HDRViewingVersion;
            Renderer::FOutputTransformSettings Candidate;
            Renderer::FResolvedOutputTransformSettings Resolved;
            RHI::ERHIFormat Format;
            Core::FString CapabilityReason;
            if (!ResolveOutput(Current,Candidate,Resolved,Format,CapabilityReason) ||
                Resolved.ReferenceWhiteNits != OutputResolved.ReferenceWhiteNits || !Status.bPrepared ||
                !Status.Capabilities.SupportsPair(Status.ResolvedState.Format,Status.ResolvedState.ColorSpace))
                bCapabilityRecovery = true;
            if (bCapabilityRecovery)
            {
                // Finish only the lifecycle drain. The settings controller
                // resolves requested intent against the new capabilities and
                // owns fallback/pause before any new target is admitted.
                Progress(true);
                if (BusySlots() != 0) return Out;
                bNeedsResize = false; bReconfigurationStarted = false;
                Out.bCompleted = true; Out.Status = EInteractiveLabServiceStatus::Success;
                return Out;
            }
            // Focus/restore notifications can change the Application display
            // generation without changing native output. Preserve in-flight
            // acquisitions: canceling them here can exhaust acquire history.
            if (!bNeedsResize && CurrentExtent == Request.Transition.DrawableExtent &&
                !bReconfigurationStarted)
            {
                Out.bCompleted = true; Out.Status = EInteractiveLabServiceStatus::Success;
                return Out;
            }
            bReconfigurationStarted = true;
            Progress(true);
            if (BusySlots() != 0) return Out;
            if (Backend->QueryLabPresentation(Status) != ERHIResult::Success)
            { Fail("lab transition capability query failed"); Out.Status = EInteractiveLabServiceStatus::Failed; return Out; }
            auto Change = PresentationRequest(OutputResolved, Request.Transition.DrawableExtent);
            Change.PreferredFormat = PresentationFormat;
            Change.SurfaceCapabilityGeneration = Status.Capabilities.CapabilityGeneration;
            Core::FString Reason;
            const auto Result = Backend->ReconfigureLabPresentation(Change, Status, &Reason);
            if (Pending(Result)) return Out;
            const auto FrameResult = Result == ERHIResult::Success
                ? Frames->Reconfigure(Change.Width, Change.Height, &Reason) : Result;
            if (FrameResult != ERHIResult::Success)
            {
                FailOperation("reconfigure", FrameResult, Reason); Out.Status = EInteractiveLabServiceStatus::Failed;
                Out.FirstFailure = FirstFailure; return Out;
            }
            CurrentExtent = Request.Transition.DrawableExtent;
            bNeedsResize = false;
            bReconfigurationStarted = false;
            Out.bCompleted = true; Out.Status = EInteractiveLabServiceStatus::Success;
            return Out;
        }
        if (Request.Phase == EInteractiveLabServicePhase::Drain)
        {
            Progress(true);
            ConsumeCaptures(true);
            Out.RetainedOwnerCount = BusySlots() + Presentations.size() + (Backend->GetDevice() ? 1 : 0);
            Out.bCompleted = BusySlots() == 0 && Frames->Snapshot().Captures.Requests==0 &&
                (Presentations.empty() || Status.RetirementMode == RHI::ERHIPresentationRetirementMode::AcquireHistory);
            Out.Status = Out.bCompleted ? EInteractiveLabServiceStatus::Success : EInteractiveLabServiceStatus::NotReady;
            Out.FirstFailure = FirstFailure;
            return Out;
        }
        if (!Request.bTerminalOnly) { Out.Status = EInteractiveLabServiceStatus::Invalid; return Out; }
        Out.RetainedOwnerCount = BusySlots() + Presentations.size() + 1;
        // Publication owns CPU bytes only, but keep its charged staging alias
        // until it finishes. The session watchdog bounds a blocked filesystem.
        if (CaptureTask.IsActive())
        {
            Core::FString Result;
            if (!CaptureTask.Poll(Result)) return Out;
            CaptureStatus=std::move(Result); CaptureStaging.reset();
        }
        if (!bDeviceClosed)
        {
            if (!bRecordedPreShutdown)
            {
                (void)Backend->QueryLabPresentation(BeforeShutdown);
                bRecordedPreShutdown = true;
            }
            const bool HadDevice = Backend->GetDevice() != nullptr;
            const auto Result = HadDevice ? Backend->Shutdown() : ERHIResult::Success;
            FDemoLabPresentationStatus Terminal;
            if (Backend->QueryLabPresentation(Terminal) == ERHIResult::Success && Terminal.bTerminalDrainComplete)
            {
                AfterShutdown = Terminal;
                Assurance = Terminal.ShutdownAssurance;
                bDeviceClosed = true;
            }
            else if (!Backend->GetDevice() && Result == ERHIResult::Success && !bSceneReady)
            {
                Assurance = RHI::ERHIShutdownAssurance::Proven;
                bDeviceClosed = true;
            }
            if (Result != ERHIResult::Success && !Pending(Result)) Fail("lab native terminal cleanup failed");
            if (!bDeviceClosed) { Out.FirstFailure = FirstFailure; return Out; }
        }
        Core::FString Reason;
        if (Frames->ReleaseAfterDeviceShutdown(Assurance, &Reason) != ERHIResult::Success)
        { Fail(Reason); Out.FirstFailure = FirstFailure; return Out; }
        ConsumeCaptures(true);
        if (Frames->Snapshot().Captures.Requests)
        { Out.Status=EInteractiveLabServiceStatus::NotReady; return Out; }
        Slots = {};
        Presentations.clear();
        UI.reset(); UIShaders = {};
        Scene.reset(); Closure = {};
        if (Assets && Assets->Shutdown() != Asset::EAssetResult::Success)
        { Fail("lab cooked session shutdown failed"); Out.FirstFailure = FirstFailure; return Out; }
        Assets.reset();
        Out.bCompleted = true; Out.RetainedOwnerCount = 0;
        Out.Status = FirstFailure.IsEmpty() ? EInteractiveLabServiceStatus::Success : EInteractiveLabServiceStatus::Failed;
        Out.FirstFailure = FirstFailure;
        switch (Assurance)
        {
        case RHI::ERHIShutdownAssurance::Proven:
            Out.ShutdownAssurance = Application::EInteractiveLabShutdownAssurance::Proven; break;
        case RHI::ERHIShutdownAssurance::IdleAssumed:
            Out.ShutdownAssurance = Application::EInteractiveLabShutdownAssurance::IdleAssumed; break;
        case RHI::ERHIShutdownAssurance::DeviceLost:
            Out.ShutdownAssurance = Application::EInteractiveLabShutdownAssurance::DeviceLost;
            Out.bDeviceLost = true; Out.Status = EInteractiveLabServiceStatus::DeviceLost; break;
        default: Out.bCompleted = false; Out.Status = EInteractiveLabServiceStatus::Failed; break;
        }
        return Out;
    }

    Core::TSharedPtr<IDemoBackendRuntime> Backend;
    Core::TSharedPtr<FLabProductionFrameContext> Frames;
    Core::TSharedPtr<FProductionContentSession> Assets;
    FProductionContentLoadedClosure Closure;
    Asset::FAssetTargetProfileEvidence TargetEvidence;
    FInteractiveLabShaders UIShaders;
    Core::TSharedPtr<Renderer::FUIRenderSession> UI;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> Scene;
    FProductionContentComposition Composition;
    std::optional<Application::FLabSettingsTransaction> ModeTransaction;
    Core::uint64 ModeFormerGeneration = 0;
    Renderer::FOutputTransformSettings OutputSettings;
    Renderer::FResolvedOutputTransformSettings OutputResolved;
    FDemoLabPresentationStatus Status, BeforeShutdown, AfterShutdown;
    RHI::FRHIPresentationCapabilities ObservedCapabilities;
    bool bCapabilityRecovery = false;
    bool bRecordedPreShutdown = false;
    bool bReconfigurationStarted = false;
    std::array<FSlot, 2> Slots;
    Core::TArray<FPresentation> Presentations;
    FWindowExtent CurrentExtent;
    RHI::ERHIFormat PresentationFormat = RHI::ERHIFormat::Unknown;
    struct FCaptureExport { Core::uint64 Id=0; Core::FString Stem; };
    std::array<FCaptureExport,2> CaptureExports;
    FLabCaptureExportTask CaptureTask;
    Core::TSharedPtr<RHI::IRHIBuffer> CaptureStaging;
    Application::FLabPresetStoreConfig CaptureStore;
    EDemoGraphicsBackend CaptureBackend=EDemoGraphicsBackend::Vulkan;
    Core::FString CaptureStatus;
    Core::uint64 NextCapture=1, CaptureServiceFrame=0;
    Core::uint64 NextToken = 1;
    Core::uint32 Submitted = 0, Completed = 0, Presented = 0, CancelledSubmissions = 0;
    Core::uint32 UIFramesSubmitted = 0, UISceneFallbackFrames = 0, DiagnosticFramesSubmitted = 0;
    Core::uint64 LastRecordedSettingsRevision = 0;
    float LastRecordedExposureStops = 0;
    float LastRecordedUIWhiteMultiplier = 0;
    Core::uint64 LastRecordedUIFrameToken = 0;
    Renderer::FUICompositionSettings LastRecordedUISettings;
    Core::uint32 LastRecordedUIOutputTransferCount = 0;
    Core::FString LastRecordedTransformVersion;
    Core::FString FirstFailure;
    RHI::ERHIShutdownAssurance Assurance = RHI::ERHIShutdownAssurance::Unknown;
    bool bNeedsResize = false;
    bool bSceneReady = false;
    bool bDeviceClosed = false;
};
} // namespace

FInteractiveLabRunResult RunInteractiveLab(
    const FDemoConfiguration& Config, const IDemoBackendFactory& Factory,
    FInteractiveLabWindowService WindowService, FInteractiveLabSessionService SessionService)
{
    FInteractiveLabRunResult Out;
    if (!Config.bInteractiveLab || !Config.IsValid(&Out.FirstFailure))
    { Out.ExitCode = EDemoExitCode::InvalidConfiguration; return Out; }
    FProductionCameraPreset Preset;
    if (!ResolveProductionCameraPreset(Config.WorkloadRevision, Preset, &Out.FirstFailure)) return Out;
    Application::FWindow Window;
    Application::FWindowDesc Desc;
    Desc.Title = "Stoner Interactive Rendering Lab";
    Desc.ClientWidth = Config.ClientWidth; Desc.ClientHeight = Config.ClientHeight;
    if (Window.CreateRealWindow(Desc) != Application::EApplicationResult::Success)
    { Out.ExitCode = EDemoExitCode::RuntimeUnavailable; Out.FirstFailure = "native lab window unavailable"; return Out; }
    Application::FFreeCameraState Camera;
    if (!InitialCamera(Preset, Window.GetDisplayState().DrawableExtent, Camera))
    { Out.FirstFailure = "initial lab drawable or camera is unsupported"; (void)Window.Destroy(); return Out; }
    auto Created = Factory.Create(Config.GraphicsBackend);
    if (!Created.Succeeded())
    { Out.ExitCode = EDemoExitCode::RuntimeUnavailable; Out.FirstFailure = Created.FailureReason; (void)Window.Destroy(); return Out; }
    auto Owner = Core::MakeShared<FLabRunOwner>(Core::TSharedPtr<IDemoBackendRuntime>(std::move(Created.Runtime)));
    Application::FInputManager Input;
    Application::FInteractiveLabSession Session;
    Application::FInteractiveLabSessionCallbacks Callbacks;
    Callbacks.Service = [Owner](const auto& Request) { return Owner->Service(Request); };
    if (Session.Initialize(Window, Input, Camera, std::move(Callbacks)) != Application::EApplicationResult::Success)
    { Out.FirstFailure = "lab session initialization failed"; (void)Window.Destroy(); return Out; }
    bool Started = false;
    try
    {
        const auto Result = Owner->Backend->InitializeLab(Window.GetPlatformWindow(), 2,
            Config.bEnableValidationLayers, Config.bLabForceAcquireHistory);
        if (Result != ERHIResult::Success)
        { Out.ExitCode = EDemoExitCode::RuntimeUnavailable; Owner->Fail("native lab backend initialization failed"); }
        else Started = Owner->Load(Config, Window.GetDisplayState().DrawableExtent);
        if (Started)
        {
            Application::FInteractiveLabUICallbacks UICallbacks;
            UICallbacks.PreflightEnable = [Owner,SessionId=Session.GetSessionId()] { return Owner->EnableUI(SessionId); };
            UICallbacks.BeginFrame = [Owner](Core::uint64 Id,bool Eligible) {
                if (Owner->UI) Owner->UI->BeginEligibleFrame(Id,Eligible);
            };
            UICallbacks.PrepareTexture = [Owner](const auto& Request) {
                return Owner->UI ? Owner->UI->PrepareTexture(Request) : Renderer::FUITextureResult{};
            };
            UICallbacks.AcquireTexture = [Owner](Renderer::FUITextureId Id) {
                return Owner->UI ? Owner->UI->AcquireTexture(Id) : Renderer::FUITextureLease{};
            };
            if (Session.ConfigureUI(std::move(UICallbacks),Config.bLabUI) != Application::EApplicationResult::Success)
            { Owner->Fail("UI-on startup requires both UI shaders in the selected cooked generation"); Started = false; }
            else if (!Owner->ConfigureSessionSettings(Session))
            { Owner->Fail("initial lab settings could not match native output"); Started = false; }
            else if (!Session.ConfigurePresetWorkload({Config.WorkloadRevision,Config.ProductionRoot,Owner->Closure.SourceIdentity.ToLowerHex()}))
            { Owner->Fail("preset workload identity could not be registered"); Started = false; }
            else if (!ConfigureLabExports(Session,Config,Owner->FirstFailure,&Owner->CaptureStore))
            { Started = false; }
            else if (!Config.LabPresetInput.IsEmpty() && !Session.RequestPresetFile(Config.LabPresetInput))
            { std::cerr << "InteractiveLab preset rejected: " << Session.GetPresetFailure().CStr() << std::endl; }
            else std::cout << "InteractiveLab: F1 toggles UI; WASD/QE move; Shift accelerates; RMB looks; Escape cancels interaction." << std::endl;
        }
    }
    catch (const std::exception& Error) { Owner->Fail(Core::FString(Error.what())); }
    if (!Started) (void)Session.RequestExit(Owner->FirstFailure);
    Owner->CaptureBackend=Config.GraphicsBackend;
    if (Started)
        (void)Session.ConfigureCaptureActions({
            [Owner,&Session](const Core::FString& Name,bool IncludeUI,bool Numeric) { return Owner->RequestCapture(Session,Name,IncludeUI,Numeric); },
            [Owner] { return Owner->CaptureStatus; }});
    auto Previous = Clock::now();
    auto LastProgress = Previous;
    Core::uint32 LastPresented = 0;
    Core::FString LastUIFailure, LastSettingsFailure;
    const auto EventThreadOwnsBackend = [&Session] {
        const auto Current = Session.GetState();
        return Current == EInteractiveLabSessionState::Running || Current == EInteractiveLabSessionState::Ready ||
            Current == EInteractiveLabSessionState::PausedZeroExtent || Current == EInteractiveLabSessionState::TransitionPending;
    };
    while (Session.GetState() != EInteractiveLabSessionState::Closed)
    {
        const auto Now = Clock::now();
        const auto Delta = std::chrono::duration<double>(Now - Previous).count();
        Previous = Now;
        try
        {
            if (WindowService && EventThreadOwnsBackend()) WindowService(Window, Owner->Presented);
            const bool CapabilitiesChanged = EventThreadOwnsBackend() && Owner->ObserveOutputCapabilities(Window);
            if (EventThreadOwnsBackend() && !CapabilitiesChanged && !Owner->bCapabilityRecovery && !Session.IsSettingsPaused() && (Session.GetState() == EInteractiveLabSessionState::Running ||
                    Session.GetState() == EInteractiveLabSessionState::Ready)) Owner->Progress(false);
            if (EventThreadOwnsBackend() && Owner->bSceneReady)
            {
                Application::FLabRuntimeInfo Info;
                Info.Workload = Config.WorkloadRevision; Info.RootIdentity = Config.ProductionRoot;
                Info.CookedGeneration = Config.StrictGeneration;
                Info.RequestedProfile = Session.GetRequestedSettings()
                    ? Session.GetRequestedSettings()->RequestedProfileId : Owner->OutputSettings.OutputDeviceProfileId;
                Info.EffectiveProfile = Owner->OutputResolved.OutputDeviceProfileId;
                Info.TransformVersion = Owner->OutputResolved.TransformStrategyVersion;
                Info.ExposureStops = Owner->OutputResolved.ManualExposureStops;
                Info.Submitted = Owner->Submitted; Info.RenderCompleted = Owner->Completed;
                Info.PresentQueued = Owner->Presented; Info.UIFrames = Owner->UIFramesSubmitted;
                Info.SceneFallbackFrames = Owner->UISceneFallbackFrames; Info.Failure = Owner->FirstFailure;
                (void)Session.UpdateRuntimeInfo(Info);
            }
            if (EventThreadOwnsBackend()) Owner->ConsumeCaptures();
            (void)Session.Service(Delta,EventThreadOwnsBackend() && Owner->CanPrepareUI());
            if (SessionService && EventThreadOwnsBackend()) SessionService(Session,Owner->Presented);
            if (Session.GetSettingsFailure() != LastSettingsFailure)
            {
                LastSettingsFailure = Session.GetSettingsFailure();
                if (!LastSettingsFailure.IsEmpty()) std::cerr << "InteractiveLab settings: " << LastSettingsFailure.CStr() << std::endl;
            }
            if (Session.GetUIFailure() != LastUIFailure)
            {
                LastUIFailure = Session.GetUIFailure();
                if (!LastUIFailure.IsEmpty()) std::cerr << "InteractiveLab UI: " << LastUIFailure.CStr() << std::endl;
            }
        }
        catch (const std::exception& Error)
        { (void)Session.RequestExit(Core::FString(Error.what())); }
        const auto State = Session.GetState();
        // RequestExit transfers exclusive backend access to the worker.
        if (EventThreadOwnsBackend())
        {
            try
            {
                Owner->ApplySessionSettings(Session, Config.IsBounded() ? Config.FrameBudget : 0);
                if (State == EInteractiveLabSessionState::Running || State == EInteractiveLabSessionState::Ready)
                {
                    if (!Owner->ModeTransaction && !Owner->bCapabilityRecovery)
                        Owner->Admit(Session, Config.IsBounded() ? Config.FrameBudget : 0);
                    if (Owner->bNeedsResize)
                    {
                        const auto& Display = Session.GetDisplayState();
                        (void)Session.RequestTransition({0, Display.DisplayGeneration, Display.DrawableExtent});
                    }
                    if (Owner->Presented != LastPresented) { LastPresented = Owner->Presented; LastProgress = Now; }
                    if (Config.IsBounded() && Owner->Presented >= Config.FrameBudget)
                        (void)Session.RequestExit();
                    else if (Session.IsSettingsPaused() && !Owner->ModeTransaction) LastProgress = Now;
                    else if (Now - LastProgress > std::chrono::seconds(5))
                        (void)Session.RequestExit("lab frame progress timed out");
                }
                else if (State == EInteractiveLabSessionState::PausedZeroExtent)
                {
                    // Keep the bounded borrowed targets across a pause. A
                    // canceled Vulkan acquisition cannot simply be reacquired.
                    Owner->Progress(false, true);
                    LastProgress = Now;
                }
                else LastProgress = Now;
                if (EventThreadOwnsBackend() && !Owner->FirstFailure.IsEmpty())
                    (void)Session.RequestExit(Owner->FirstFailure);
            }
            catch (const std::exception& Error)
            { (void)Session.RequestExit(Core::FString(Error.what())); }
        }
        if (Session.GetState() != EInteractiveLabSessionState::Closed)
        {
            const auto Wait = Session.GetState() == EInteractiveLabSessionState::PausedZeroExtent ? 50u : 1u;
            std::this_thread::sleep_for(std::chrono::milliseconds(Wait));
        }
    }
    // Closed is published after the worker/watchdog join, making these final
    // value reads safe. Window destruction happens only after native cleanup.
    Out.SubmittedFrames = Owner->Submitted;
    Out.RenderCompletedFrames = Owner->Completed;
    Out.PresentedFrames = Owner->Presented;
    Out.LastRecordedSettingsRevision = Owner->LastRecordedSettingsRevision;
    Out.LastRecordedExposureStops = Owner->LastRecordedExposureStops;
    Out.LastRecordedUIWhiteMultiplier = Owner->LastRecordedUIWhiteMultiplier;
    Out.LastRecordedUIFrameToken = Owner->LastRecordedUIFrameToken;
    Out.LastRecordedUISettings = Owner->LastRecordedUISettings;
    Out.LastRecordedUIOutputTransferCount = Owner->LastRecordedUIOutputTransferCount;
    Out.LastRecordedTransformVersion = Owner->LastRecordedTransformVersion;
    Out.UIFramesSubmitted = Owner->UIFramesSubmitted;
    Out.DiagnosticFramesSubmitted = Owner->DiagnosticFramesSubmitted;
    Out.UISceneFallbackFrames = Owner->UISceneFallbackFrames;
    Out.FinalFrameState = Owner->Frames->Snapshot();
    Out.BeforeNativeShutdown = Owner->BeforeShutdown;
    Out.AfterNativeShutdown = Owner->AfterShutdown;
    Out.ShutdownAssurance = Owner->Assurance;
    Out.FirstFailure = Session.GetFirstFailure();
    if (Out.FirstFailure.IsEmpty()) Out.FirstFailure = Owner->FirstFailure;
    Out.ExitCode = Out.FirstFailure.IsEmpty() ? EDemoExitCode::Success :
        (Started ? EDemoExitCode::FrameFailed : Out.ExitCode);
    std::cout << "InteractiveLab preview: submitted=" << Out.SubmittedFrames
        << " render-completed=" << Out.RenderCompletedFrames << " present-queued=" << Out.PresentedFrames
        << " diagnostic-submitted=" << Out.DiagnosticFramesSubmitted << " ui-submitted=" << Out.UIFramesSubmitted << " ui-scene-fallback=" << Out.UISceneFallbackFrames
        << " shutdown=" << Application::FInteractiveLabSession::ToString(Session.GetShutdownAssurance()) << '\n';
    const auto& LiveOps = Out.BeforeNativeShutdown.RuntimeSnapshot.NativeOperations;
    const auto& FinalOps = Out.AfterNativeShutdown.RuntimeSnapshot.NativeOperations;
    const auto& Native = Out.AfterNativeShutdown.RuntimeSnapshot.NativePresentation;
    std::cout << "InteractiveLab native: available=" << LiveOps.bAvailable
        << " image-readback-copies=" << LiveOps.ImageReadbackCopyCount
        << " readback-maps=" << LiveOps.ReadbackMapCount << " readback-waits=" << LiveOps.ReadbackWaitCount
        << " live-queue-idles=" << LiveOps.QueueIdleCallCount << " live-device-idles=" << LiveOps.DeviceIdleCallCount
        << " terminal-device-idles=" << Native.TerminalIdleCallCount
        << " terminal-idle-ns=" << Native.TerminalIdleNanoseconds
        << " proven-present-releases=" << FinalOps.ProvenPresentationReleaseCount
        << " render-retired=" << Out.FinalFrameState.RenderRetiredFrameCount
        << " final-presentation-owners=" << Native.PresentationOwnerCount << '\n';
    if (!Out.FirstFailure.IsEmpty()) std::cerr << "InteractiveLab failed: " << Out.FirstFailure.CStr() << '\n';
    (void)Window.Destroy();
    return Out;
}
} // namespace Stoner::Demo
