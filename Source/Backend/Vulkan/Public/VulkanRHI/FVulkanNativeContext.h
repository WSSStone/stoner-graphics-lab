#pragma once

#include "RHI/RHIMinimal.h"
#include "Core/FPlatformWindow.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanComputePipeline;
class FVulkanGraphicsPipeline;
class FVulkanShaderModule;
class FVulkanTexture;
struct FVulkanNativeDeviceAccess;
class FVulkanNativeOffscreenSession;

enum class EVulkanDeferredProbeMetric
{
    Absolute,
    NormalDot
};

enum class EVulkanDeferredFailurePoint
{
    None,
    PartialInitialization,
    Record,
    Submit,
    Fence,
    Copy,
    Map,
    Decode,
    Probe
};

enum class EVulkanVisibleFrameFailurePoint
{
    None,
    AcquireSuboptimal,
    Record,
    SubmitAfterFenceReset
};

struct FVulkanDeferredProbe
{
    Stoner::Core::FString Convention;
    Stoner::Core::FString Name;
    Stoner::Core::FString Semantic;
    Stoner::Core::uint32 X = 0;
    Stoner::Core::uint32 Y = 0;
    Stoner::Core::FVector4 Expected;
    Stoner::Core::FVector4 Observed;
    float Threshold = 0.0f;
    float ErrorMeasure = 0.0f;
    EVulkanDeferredProbeMetric Metric = EVulkanDeferredProbeMetric::Absolute;
    bool bPassed = false;
};

struct FVulkanDeferredValidationReport
{
    Stoner::Core::FString RuntimeMode;
    Stoner::Core::FString AdapterIdentity;
    Stoner::Core::FString ReferencePath;
    Stoner::Core::TArray<FVulkanDeferredProbe> Probes;
    Stoner::Core::uint32 PeakLiveObjects = 0;
    Stoner::Core::uint32 FinalLiveObjects = 0;
    EVulkanDeferredFailurePoint InjectedFailure = EVulkanDeferredFailurePoint::None;
    Stoner::Core::FString PrimaryFailureStage;
    Stoner::Core::uint32 CompletedStageCount = 0;
    bool bSoftwareDevice = false;
    bool bNativeSubmissionCompleted = false;
    bool bPassed = false;

    [[nodiscard]] Stoner::Core::uint32 GetProbeCount(
        const Stoner::Core::FString& Convention) const noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;
};

struct FVulkanNativeFrameBindings
{
    Stoner::Core::uint32 ImageIndex = 0;
    Stoner::Core::uint32 FrameSlot = 0;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> OutputTexture;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> VertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline> GraphicsPipeline;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass> RenderPass;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer> Framebuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer> CommandBuffer;
};

struct FVulkanVisibleFrameFailureReport
{
    EVulkanVisibleFrameFailurePoint InjectedFailure =
        EVulkanVisibleFrameFailurePoint::None;
    Stoner::RHI::ERHIResult FirstResult = Stoner::RHI::ERHIResult::Success;
    Stoner::RHI::ERHIResult NextAcquireResult = Stoner::RHI::ERHIResult::Success;
    bool bAcquiredStateReleased = false;
    bool bFenceReadyForReuse = false;
    bool bPassed = false;
};

class FVulkanNativeContext
{
public:
    FVulkanNativeContext();
    ~FVulkanNativeContext();
    FVulkanNativeContext(const FVulkanNativeContext&) = delete;
    FVulkanNativeContext& operator=(const FVulkanNativeContext&) = delete;

    [[nodiscard]] Stoner::RHI::ERHIResult Initialize(Stoner::RHI::ERHIRuntimeMode Mode,
        const Stoner::Core::FPlatformWindow& PlatformWindow = {});
    [[nodiscard]] Stoner::RHI::ERHIResult ExecuteOffscreenTriangle(
        const Stoner::Core::FString& VertexShaderPath,
        const Stoner::Core::FString& FragmentShaderPath);
    [[nodiscard]] Stoner::RHI::ERHIResult ExecuteDeferredOffscreenValidation(
        const Stoner::Core::FString& ShaderDirectory,
        FVulkanDeferredValidationReport& OutReport,
        EVulkanDeferredFailurePoint FailurePoint = EVulkanDeferredFailurePoint::None);
    [[nodiscard]] static FVulkanDeferredValidationReport
    RunDeferredFailureLifecycleValidation(EVulkanDeferredFailurePoint FailurePoint) noexcept;
    [[nodiscard]] static FVulkanVisibleFrameFailureReport
    RunVisibleFrameFailureLifecycleValidation(
        EVulkanVisibleFrameFailurePoint FailurePoint) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult PrepareVisibleTriangle(
        const Stoner::Core::FString& VertexShaderPath,
        const Stoner::Core::FString& FragmentShaderPath,
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height);
    [[nodiscard]] Stoner::RHI::ERHIResult AcquireVisibleFrame(FVulkanNativeFrameBindings& OutBindings);
    [[nodiscard]] Stoner::RHI::ERHIResult SubmitAndPresentVisibleFrame(const FVulkanNativeFrameBindings& Bindings);
    [[nodiscard]] Stoner::RHI::ERHIResult DrawVisibleFrame();
    [[nodiscard]] Stoner::RHI::ERHIResult RecreateVisiblePresentation(
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height);
    [[nodiscard]] Stoner::RHI::ERHIResult Shutdown();
    [[nodiscard]] const Stoner::RHI::FRHIRuntimeSnapshot& GetSnapshot() const noexcept;
    [[nodiscard]] bool IsAvailable() const noexcept;

private:
    friend class FVulkanDevice;
    friend class FVulkanComputePipeline;
    friend class FVulkanGraphicsPipeline;
    friend class FVulkanShaderModule;
    friend class FVulkanTexture;
    friend class FVulkanNativeOffscreenSession;
    [[nodiscard]] bool GetNativeDeviceAccess(
        FVulkanNativeDeviceAccess& OutAccess) const noexcept;
    [[nodiscard]] Stoner::Core::TArray<
        Stoner::RHI::FRHIFormatCapabilities>
        QueryTextureFormatCapabilities() const;
    [[nodiscard]] Stoner::RHI::ERHIResult CreateOwnedShaderModule(
        const Stoner::Core::TArray<Stoner::Core::uint32>& Words,
        Stoner::Core::uint64& OutToken) noexcept;
    void DestroyOwnedShaderModule(Stoner::Core::uint64 Token) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult CreateOwnedGraphicsPipeline(
        const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc,
        Stoner::Core::uint64 VertexShaderToken,
        Stoner::Core::uint64 FragmentShaderToken,
        Stoner::Core::uint64& OutToken) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult CreateOwnedComputePipeline(
        const Stoner::RHI::FRHIComputePipelineDesc& Desc,
        Stoner::Core::uint64 ComputeShaderToken,
        Stoner::Core::uint64& OutToken) noexcept;
    void DestroyOwnedPipeline(Stoner::Core::uint64 Token) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult CreateOwnedTexture(
        const Stoner::RHI::FRHITextureDesc& Desc,
        Stoner::Core::uint64& OutToken) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult UploadOwnedTexture(
        Stoner::Core::uint64 Token,
        const Stoner::RHI::FRHITextureUploadDesc& Upload) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ReadbackOwnedTexture(
        Stoner::Core::uint64 Token,
        Stoner::Core::uint32 MipLevel,
        Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes) noexcept;
    void DestroyOwnedTexture(Stoner::Core::uint64 Token) noexcept;
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};

[[nodiscard]] const char* ToString(EVulkanDeferredFailurePoint FailurePoint) noexcept;
[[nodiscard]] const char* ToString(EVulkanVisibleFrameFailurePoint FailurePoint) noexcept;

} // namespace Stoner::Backend::Vulkan
