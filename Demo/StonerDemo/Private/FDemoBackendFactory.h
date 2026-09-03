#pragma once

#include "Core/CoreMinimal.h"
#include "FDemoConfiguration.h"
#include "RHI/FRHIRuntimeSnapshot.h"
#include "RHI/FRHIPresentationCapabilities.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/FRHISwapchainDesc.h"
#include "RHI/FRHIShaderModuleDesc.h"
#include "RHI/ERHIResult.h"
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
