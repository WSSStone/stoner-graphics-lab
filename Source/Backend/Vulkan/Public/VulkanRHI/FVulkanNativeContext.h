#pragma once

#include "RHI/RHIMinimal.h"
#include "Core/FPlatformWindow.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

struct FVulkanNativeDeviceAccess;
class FVulkanNativeOffscreenSession;

enum class EVulkanDeferredProbeMetric
{
    Absolute,
    NormalDot
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
        FVulkanDeferredValidationReport& OutReport);
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
    friend class FVulkanNativeOffscreenSession;
    [[nodiscard]] bool GetNativeDeviceAccess(
        FVulkanNativeDeviceAccess& OutAccess) const noexcept;
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};

} // namespace Stoner::Backend::Vulkan
