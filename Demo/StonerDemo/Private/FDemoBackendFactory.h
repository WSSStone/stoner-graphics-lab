#pragma once

#include "Core/CoreMinimal.h"
#include "FDemoConfiguration.h"
#include "RHI/FRHIRuntimeSnapshot.h"
#include "RHI/FRHIPresentationCapabilities.h"
#include "RHI/FRHIPresentationFrame.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/FRHISwapchainDesc.h"
#include "RHI/FRHIShaderModuleDesc.h"
#include "RHI/ERHIPresentationRetirement.h"
#include "RHI/ERHIResult.h"
#include "RHI/IRHIFence.h"
#include "RHI/IRHISwapchain.h"
#include "Renderer/FForwardFrameExecutor.h"

#include <span>

namespace Stoner::RHI
{
class IRHIDevice;
}

namespace Stoner::Demo
{

struct FDemoBackendFrame
{
    Renderer::FForwardFrameExecutionBindings ExecutionBindings;
    Core::uint32 FrameIndex = 0;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
};

struct FDemoProductionPresentationResult
{
    Core::TArray<Core::uint8> Rgba8;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    Core::uint32 RowPitchBytes = 0;
    Core::uint64 FrameToken = 0;
    RHI::ERHIFormat Format = RHI::ERHIFormat::Unknown;
    RHI::FRHIResolvedPresentationState ResolvedState;
    Core::FString CapabilityDigest;
    Core::FString FailureStage;
    bool bPresented = false;
};

// Backend-neutral state for the interactive lab presentation path.  The
// runtime snapshot and capability/resolved objects remain the authoritative
// source for device and presentation identity.  The owner counts describe
// only records visible at this facade boundary; native backends retain any
// additional owners until their own retirement proof.
struct FDemoLabPresentationStatus
{
    RHI::FRHIPresentationCapabilities Capabilities;
    RHI::FRHIResolvedPresentationState ResolvedState;
    RHI::FRHIRuntimeSnapshot RuntimeSnapshot;
    RHI::ERHIPresentationRetirementMode RetirementMode =
        RHI::ERHIPresentationRetirementMode::Unknown;
    RHI::ERHIPresentationRetirementReason RetirementReason =
        RHI::ERHIPresentationRetirementReason::Unknown;
    RHI::ERHIShutdownAssurance ShutdownAssurance =
        RHI::ERHIShutdownAssurance::Unknown;
    Core::uint32 PendingAcquireCount = 0;
    Core::uint32 PendingPresentationLeaseCount = 0;
    Core::uint32 RetainedFacadeOwnerCount = 0;
    bool bPrepared = false;
    bool bTerminalDrainStarted = false;
    bool bTerminalDrainComplete = false;
    Core::FString FailureReason;
};

class IDemoBackendRuntime
{
public:
    virtual ~IDemoBackendRuntime() = default;

    [[nodiscard]] virtual EDemoGraphicsBackend GetBackend() const noexcept = 0;
    [[nodiscard]] virtual RHI::ERHIResult Initialize(
        EDemoRunMode Mode,
        const Core::FPlatformWindow& Window,
        Core::uint32 FramesInFlight,
        bool bEnableValidation) = 0;
    // Interactive lab startup is deliberately separate from the legacy
    // runtime initializer.  Legacy implementations retain the old path and
    // do not claim borrowed-target support by default.
    [[nodiscard]] virtual RHI::ERHIResult InitializeLab(
        const Core::FPlatformWindow&,
        Core::uint32 = 2,
        bool = false,
        bool = false)
    {
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult PrepareTriangle(
        const RHI::FRHIShaderModuleDesc& VertexShader,
        const RHI::FRHIShaderModuleDesc& FragmentShader,
        Core::uint32 Width,
        Core::uint32 Height) = 0;
    [[nodiscard]] virtual RHI::ERHIResult AcquireFrame(
        FDemoBackendFrame& OutFrame) = 0;
    [[nodiscard]] virtual RHI::ERHIResult SubmitFrame(
        const FDemoBackendFrame& Frame) = 0;
    [[nodiscard]] virtual RHI::ERHIResult RecreatePresentation(
        Core::uint32 Width,
        Core::uint32 Height) = 0;
    [[nodiscard]] virtual RHI::ERHIResult PrepareProductionPresentation(
        Core::uint32 Width,
        Core::uint32 Height) = 0;
    [[nodiscard]] virtual RHI::ERHIResult
    QueryProductionPresentationCapabilities(
        RHI::FRHIPresentationCapabilities&) const
    {
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult PrepareProductionPresentationMode(
        const RHI::FRHISwapchainDesc&,
        RHI::FRHIResolvedPresentationState*)
    {
        return RHI::ERHIResult::Unsupported;
    }
    // Borrowed-target presentation for the interactive lab.  Every default
    // remains Unsupported so an old runtime cannot fall back to formal
    // acquisition, readback, or synchronous presentation.
    [[nodiscard]] virtual RHI::ERHIResult PrepareLabPresentation(
        const RHI::FRHISwapchainDesc&,
        FDemoLabPresentationStatus& OutStatus,
        Core::FString* = nullptr)
    {
        OutStatus = {};
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult ReconfigureLabPresentation(
        const RHI::FRHISwapchainDesc&,
        FDemoLabPresentationStatus& OutStatus,
        Core::FString* = nullptr)
    {
        OutStatus = {};
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult QueryLabPresentation(
        FDemoLabPresentationStatus& OutStatus) const
    {
        OutStatus = {};
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult AcquireLabTarget(
        Core::uint64,
        Core::uint32,
        RHI::FRHIBorrowedAcquiredTarget& OutTarget,
        Core::FString* = nullptr)
    {
        OutTarget = {};
        return RHI::ERHIResult::Unsupported;
    }
    // Exact admission provenance, including a private pending acquisition.
    // Rejected/paused calls may have produced no backend owner at all.
    [[nodiscard]] virtual bool OwnsLabAcquireAttempt(Core::uint64, Core::uint32) const noexcept
    {
        return false;
    }
    [[nodiscard]] virtual RHI::ERHIResult PresentLabTarget(
        const RHI::FRHIBorrowedAcquiredTarget&,
        const RHI::FRHIRenderLease&,
        RHI::FRHIPresentationLease& OutLease,
        Core::FString* = nullptr)
    {
        OutLease = {};
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult CancelLabTarget(
        Core::uint64,
        Core::uint32,
        const Core::TSharedPtr<RHI::IRHIFence>&,
        bool& bOutCancellationAcknowledged,
        Core::FString* = nullptr)
    {
        bOutCancellationAcknowledged = false;
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult PollLabPresentation(
        const RHI::FRHIPresentationLease&,
        bool& bOutPresentationComplete,
        Core::FString* = nullptr)
    {
        bOutPresentationComplete = false;
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult PresentProductionImage(
        std::span<const Core::uint8> Rgba8,
        Core::uint32 Width,
        Core::uint32 Height,
        Core::uint32 RowPitchBytes,
        FDemoProductionPresentationResult& OutResult) = 0;
    // Feature 029 authority path: exact GPU-native formal output only. The
    // legacy byte/image method remains available solely for Feature 028-era
    // preview and non-authoritative compatibility probes.
    [[nodiscard]] virtual RHI::ERHIResult PresentProductionFormalOutput(
        const Core::TSharedPtr<RHI::IRHITexture>&,
        Core::uint64,
        FDemoProductionPresentationResult&)
    {
        return RHI::ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual RHI::ERHIResult ExecuteOffscreenTriangle(
        const Renderer::FForwardFramePlan& Plan,
        const RHI::FRHIShaderModuleDesc& VertexShader,
        const RHI::FRHIShaderModuleDesc& FragmentShader) = 0;
    [[nodiscard]] virtual RHI::FRHIRuntimeSnapshot GetSnapshot() const noexcept = 0;
    [[nodiscard]] virtual Core::TSharedPtr<RHI::IRHIDevice> GetDevice() const noexcept = 0;
    [[nodiscard]] virtual RHI::ERHIResult Shutdown() = 0;
};

struct FDemoBackendCreateResult
{
    RHI::ERHIResult Result = RHI::ERHIResult::Failed;
    EDemoGraphicsBackend RequestedBackend = EDemoGraphicsBackend::Vulkan;
    EDemoGraphicsBackend SelectedBackend = EDemoGraphicsBackend::Vulkan;
    Core::TUniquePtr<IDemoBackendRuntime> Runtime;
    Core::FString FailureReason;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == RHI::ERHIResult::Success && Runtime != nullptr &&
            RequestedBackend == SelectedBackend;
    }
};

class IDemoBackendFactory
{
public:
    virtual ~IDemoBackendFactory() = default;
    [[nodiscard]] virtual FDemoBackendCreateResult Create(
        EDemoGraphicsBackend Backend) const = 0;
};

class FDemoBackendFactory final : public IDemoBackendFactory
{
public:
    [[nodiscard]] FDemoBackendCreateResult Create(
        EDemoGraphicsBackend Backend) const override;
};

} // namespace Stoner::Demo
