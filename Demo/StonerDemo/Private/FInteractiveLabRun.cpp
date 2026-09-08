#include "FInteractiveLabRun.h"

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
        if (!Session.ConfigureSettings(Initial,Caps)) return false;
        Application::FLabControlSection Outputs;
        Outputs.Id = "OutputProfiles"; Outputs.Title = "Output profiles";
        for (const auto& Output : Caps.Outputs)
        {
            const auto Id = Output.ProfileId;
            Outputs.Commands.push_back({Id,Id,[Id](Application::FLabSettingsSnapshot& Edit) {
                Edit.RequestedProfileId = Id; return true;
            }});
        }
        return Session.RegisterControlSection(Outputs);
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
                Caps.Outputs.push_back({Profile.ProfileId,Resolved.ReferenceWhiteNits,Resolved.ReferenceWhiteNits});
        }
        return Caps;
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
            if (Updated == ERHIResult::Success && Status.bPrepared)
            {
                if (Session.CompleteSettingsTransaction(Transaction->Token,true,true))
                { OutputSettings = std::move(Candidate); OutputResolved = Resolved; }
                return;
            }
            if (Updated != ERHIResult::ResizeRequired && Updated != ERHIResult::Success)
            { (void)Session.CompleteSettingsTransaction(Transaction->Token,false,Status.bPrepared); return; }
            ModeTransaction = *Transaction;
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
        { (void)Session.CompleteSettingsTransaction(Token,false,Status.bPrepared); return; }
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
                if (Frames->PollPresentation(Record.Lease.Frame.FrameToken, Record.Slot, Retired, &Reason) !=
                    ERHIResult::Success || !Retired) { Fail(Reason); ++Index; }
                else Presentations.erase(Presentations.begin() + Index);
            }
            else
            {
                if (Result != ERHIResult::Success && !Pending(Result)) Fail(Reason);
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
        for (Core::uint32 Index = 0; Index < Slots.size(); ++Index)
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
                    if (!Pending(Reserved)) Fail(Reason);
                    return;
                }
            }
            Core::FString Reason;
            const auto Acquired = Backend->AcquireLabTarget(Slot.Token, Index, Slot.Target, &Reason);
            Slot.bAcquireAttempted = Backend->OwnsLabAcquireAttempt(Slot.Token, Index);
            if (Acquired == ERHIResult::ResizeRequired) { bNeedsResize = true; return; }
            if (Pending(Acquired)) continue;
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
            { Fail(Reason); return; }
            View.InverseViewProjection = FrameCamera.InverseViewProjection;
            View.CameraPosition = Camera.Position;
            View.Extent = {CurrentExtent.Width, CurrentExtent.Height};
            Frame.DeferredInputs.Output.Extent = View.Extent;
            const auto RetainedBackend = Backend;
            FLabProductionFrameContext::FPrepareUI PrepareUI;
            if (Session.IsUIEnabled() && UI)
            {
                PrepareUI = [&, Token=Slot.Token](const auto& SceneInput,Core::uint64 Available,
                    Core::TSharedPtr<Renderer::FUIRenderFrame>& OutFrame) {
                    const auto& Display = Session.GetDisplayState();
                    const auto Required = static_cast<Core::uint64>(Display.DrawableExtent.Width) *
                        Display.DrawableExtent.Height * 8ULL;
                    if (Required > Available) return ERHIResult::Unavailable;
                    const auto* Effective = Session.GetEffectiveSettings();
                    const auto Revision = Effective ? Effective->SettingsRevision : 1;
                    Renderer::FUIDrawSnapshot Snapshot(Session.GetSessionId(),Token,Revision,Display.DisplayGeneration);
                    if (Session.ExtractUIDrawSnapshot(Snapshot) != ERHIResult::Success) return ERHIResult::NotReady;
                    Renderer::FUICompositionSettings Settings;
                    Settings.OutputProfileId = OutputResolved.OutputDeviceProfileId;
                    Settings.BlendDomain = OutputResolved.DisplayLinearDomain;
                    Settings.UIReferenceWhiteNits = OutputResolved.ReferenceWhiteNits;
                    Settings.NativePackingWhiteNits = Status.ResolvedState.ReferenceWhiteNits;
                    Settings.DisplayGeneration = Display.DisplayGeneration;
                    const auto Prepared = UI->PrepareFrame(Snapshot,Settings,Revision,0,SceneInput,
                        UIShaders.Draw.ModuleDescriptions,UIShaders.Copy.ModuleDescriptions,OutFrame);
                    return Prepared == ERHIResult::InvalidState || Prepared == ERHIResult::Unsupported
                        ? ERHIResult::Unavailable : Prepared;
                };
            }
            const auto Recorded = RecordLabProductionPreview(Frames, Frame, Index, Status.ResolvedState,
                [RetainedBackend](Core::uint64 Token, Core::uint32 SlotIndex,
                    const Core::TSharedPtr<RHI::IRHIFence>& Fence, bool& Acknowledged) {
                    return RetainedBackend->CancelLabTarget(Token, SlotIndex, Fence, Acknowledged);
                }, Slot.Ticket, PrepareUI);
            if (Recorded.Result != Renderer::EOutputTransformResult::Success)
            { Fail("lab preview graph recording failed"); return; }
            const auto Queued = Renderer::FOutputTransformExecutor().SubmitPreview(Slot.Ticket);
            Slot.bSubmitted = Queued.bQueued;
            if (Slot.bSubmitted)
            {
                ++Submitted;
                const auto* Resources = Frames->GetResources(Slot.Token,Index);
                if (Resources)
                {
                    LastRecordedExposureStops = Resources->OutputTransformPlan.ResolvedSettings.ManualExposureStops;
                    LastRecordedTransformVersion = Resources->OutputTransformPlan.ResolvedSettings.TransformStrategyVersion;
                    LastRecordedSettingsRevision = Session.GetEffectiveSettings() ? Session.GetEffectiveSettings()->SettingsRevision : 1;
                }
                if (Resources && Resources->OutputTransformPlan.TerminalUI) ++UIFramesSubmitted;
                else if (Session.IsUIEnabled()) ++UISceneFallbackFrames;
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
                if (!Pending(Result) && Result != ERHIResult::Success) Fail(Reason);
                return;
            }
            Slot.bCancellationAcknowledged = true;
        }
        if (Frames->GetFrameState(Slot.Token, Index) != ELabProductionFrameState::Cancelled &&
            Frames->CancelFrame(Slot.Token, Index, &Reason) != ERHIResult::Success)
        { Fail(Reason); return; }
        const auto Result = Frames->RetireCancelled(Slot.Token, Index, &Reason);
        if (Frames->GetFrameState(Slot.Token, Index) == ELabProductionFrameState::Free) Slot = {};
        if (Result != ERHIResult::Success && !Pending(Result)) Fail(Reason);
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
            Out.RetainedOwnerCount = BusySlots() + Presentations.size() + (Backend->GetDevice() ? 1 : 0);
            Out.bCompleted = BusySlots() == 0 &&
                (Presentations.empty() || Status.RetirementMode == RHI::ERHIPresentationRetirementMode::AcquireHistory);
            Out.Status = Out.bCompleted ? EInteractiveLabServiceStatus::Success : EInteractiveLabServiceStatus::NotReady;
            Out.FirstFailure = FirstFailure;
            return Out;
        }
        if (!Request.bTerminalOnly) { Out.Status = EInteractiveLabServiceStatus::Invalid; return Out; }
        Out.RetainedOwnerCount = BusySlots() + Presentations.size() + 1;
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
    Renderer::FOutputTransformSettings OutputSettings;
    Renderer::FResolvedOutputTransformSettings OutputResolved;
    FDemoLabPresentationStatus Status, BeforeShutdown, AfterShutdown;
    bool bRecordedPreShutdown = false;
    bool bReconfigurationStarted = false;
    std::array<FSlot, 2> Slots;
    Core::TArray<FPresentation> Presentations;
    FWindowExtent CurrentExtent;
    RHI::ERHIFormat PresentationFormat = RHI::ERHIFormat::Unknown;
    Core::uint64 NextToken = 1;
    Core::uint32 Submitted = 0, Completed = 0, Presented = 0, CancelledSubmissions = 0;
    Core::uint32 UIFramesSubmitted = 0, UISceneFallbackFrames = 0;
    Core::uint64 LastRecordedSettingsRevision = 0;
    float LastRecordedExposureStops = 0;
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
            else std::cout << "InteractiveLab: F1 toggles UI; WASD/QE move; Shift accelerates; RMB looks; Escape cancels interaction." << std::endl;
        }
    }
    catch (const std::exception& Error) { Owner->Fail(Core::FString(Error.what())); }
    if (!Started) (void)Session.RequestExit(Owner->FirstFailure);
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
            if (EventThreadOwnsBackend() && (Session.GetState() == EInteractiveLabSessionState::Running ||
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
                    if (!Owner->ModeTransaction) Owner->Admit(Session, Config.IsBounded() ? Config.FrameBudget : 0);
                    if (Owner->bNeedsResize)
                    {
                        const auto& Display = Session.GetDisplayState();
                        (void)Session.RequestTransition({0, Display.DisplayGeneration, Display.DrawableExtent});
                    }
                    if (Owner->Presented != LastPresented) { LastPresented = Owner->Presented; LastProgress = Now; }
                    if (Config.IsBounded() && Owner->Presented >= Config.FrameBudget)
                        (void)Session.RequestExit();
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
    Out.LastRecordedTransformVersion = Owner->LastRecordedTransformVersion;
    Out.UIFramesSubmitted = Owner->UIFramesSubmitted;
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
        << " ui-submitted=" << Out.UIFramesSubmitted << " ui-scene-fallback=" << Out.UISceneFallbackFrames
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
