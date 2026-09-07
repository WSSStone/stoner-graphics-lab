#include "FInteractiveLabRun.h"

#include "Application/FInteractiveLabSession.h"
#include "Asset/FAssetCookContractCodec.h"
#include "Core/FPlatformFileSystem.h"
#include "FLabProductionPreviewExecutor.h"
#include "FProductionContentSession.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
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
        if (FirstFailure.IsEmpty()) FirstFailure = Reason.IsEmpty() ? "interactive lab operation failed" : Reason;
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
        CurrentExtent = Extent;
        bSceneReady = true;
        return true;
    }

    void Progress(bool bStop)
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
                    else if (PresentResult != ERHIResult::Success && !Pending(PresentResult)) Fail(Reason);
                    if (!Slot.bPresentQueued && FirstFailure.IsEmpty()) continue;
                }
            }
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

    void Admit(const Application::FFreeCameraState& Camera, Core::uint32 Budget)
    {
        if (!bSceneReady || !FirstFailure.IsEmpty() || (Budget && Submitted - CancelledSubmissions >= Budget)) return;
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
            if (Acquired != ERHIResult::Success) { Fail(Reason); return; }
            if (Frames->BeginFrame(Slot.Token, Index, Slot.Target, &Reason) != ERHIResult::Success)
            { Fail(Reason); return; }
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
            const auto Recorded = RecordLabProductionPreview(Frames, Frame, Index, Status.ResolvedState,
                [RetainedBackend](Core::uint64 Token, Core::uint32 SlotIndex,
                    const Core::TSharedPtr<RHI::IRHIFence>& Fence, bool& Acknowledged) {
                    return RetainedBackend->CancelLabTarget(Token, SlotIndex, Fence, Acknowledged);
                }, Slot.Ticket);
            if (Recorded.Result != Renderer::EOutputTransformResult::Success)
            { Fail("lab preview graph recording failed"); return; }
            const auto Queued = Renderer::FOutputTransformExecutor().SubmitPreview(Slot.Ticket);
            Slot.bSubmitted = Queued.bQueued;
            if (Slot.bSubmitted) ++Submitted;
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
            if (Result != ERHIResult::Success ||
                Frames->Reconfigure(Change.Width, Change.Height, &Reason) != ERHIResult::Success)
            {
                Fail(Reason); Out.Status = EInteractiveLabServiceStatus::Failed;
                Out.FirstFailure = FirstFailure; return Out;
            }
            CurrentExtent = Request.Transition.DrawableExtent;
            bNeedsResize = false;
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
            const bool HadDevice = Backend->GetDevice() != nullptr;
            const auto Result = HadDevice ? Backend->Shutdown() : ERHIResult::Success;
            FDemoLabPresentationStatus Terminal;
            if (Backend->QueryLabPresentation(Terminal) == ERHIResult::Success && Terminal.bTerminalDrainComplete)
            {
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
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> Scene;
    FProductionContentComposition Composition;
    Renderer::FOutputTransformSettings OutputSettings;
    Renderer::FResolvedOutputTransformSettings OutputResolved;
    FDemoLabPresentationStatus Status;
    std::array<FSlot, 2> Slots;
    Core::TArray<FPresentation> Presentations;
    FWindowExtent CurrentExtent;
    RHI::ERHIFormat PresentationFormat = RHI::ERHIFormat::Unknown;
    Core::uint64 NextToken = 1;
    Core::uint32 Submitted = 0, Completed = 0, Presented = 0, CancelledSubmissions = 0;
    Core::FString FirstFailure;
    RHI::ERHIShutdownAssurance Assurance = RHI::ERHIShutdownAssurance::Unknown;
    bool bNeedsResize = false;
    bool bSceneReady = false;
    bool bDeviceClosed = false;
};
} // namespace

FInteractiveLabRunResult RunInteractiveLab(
    const FDemoConfiguration& Config, const IDemoBackendFactory& Factory,
    FInteractiveLabWindowService WindowService)
{
    FInteractiveLabRunResult Out;
    if (!Config.bInteractiveLab || !Config.IsValid(&Out.FirstFailure))
    { Out.ExitCode = EDemoExitCode::InvalidConfiguration; return Out; }
    if (Config.bLabUI)
    { Out.FirstFailure = "UI-enabled lab startup requires the pending ImGui integration; use --lab-ui off"; return Out; }
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
    }
    catch (const std::exception& Error) { Owner->Fail(Core::FString(Error.what())); }
    if (!Started) (void)Session.RequestExit(Owner->FirstFailure);
    auto Previous = Clock::now();
    auto LastProgress = Previous;
    Core::uint32 LastPresented = 0;
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
            (void)Session.Service(Delta);
        }
        catch (const std::exception& Error)
        { (void)Session.RequestExit(Core::FString(Error.what())); }
        const auto State = Session.GetState();
        // RequestExit transfers exclusive backend access to the worker.
        if (EventThreadOwnsBackend())
        {
            try
            {
                if (State == EInteractiveLabSessionState::Running || State == EInteractiveLabSessionState::Ready)
                {
                    Owner->Progress(false);
                    Owner->Admit(Session.GetCameraState(), Config.IsBounded() ? Config.FrameBudget : 0);
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
                    Owner->Progress(true);
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
    Out.ShutdownAssurance = Owner->Assurance;
    Out.FirstFailure = Session.GetFirstFailure();
    if (Out.FirstFailure.IsEmpty()) Out.FirstFailure = Owner->FirstFailure;
    Out.ExitCode = Out.FirstFailure.IsEmpty() ? EDemoExitCode::Success :
        (Started ? EDemoExitCode::FrameFailed : Out.ExitCode);
    std::cout << "InteractiveLab preview: submitted=" << Out.SubmittedFrames
        << " render-completed=" << Out.RenderCompletedFrames << " present-queued=" << Out.PresentedFrames
        << " shutdown=" << Application::FInteractiveLabSession::ToString(Session.GetShutdownAssurance()) << '\n';
    if (!Out.FirstFailure.IsEmpty()) std::cerr << "InteractiveLab failed: " << Out.FirstFailure.CStr() << '\n';
    (void)Window.Destroy();
    return Out;
}
} // namespace Stoner::Demo
