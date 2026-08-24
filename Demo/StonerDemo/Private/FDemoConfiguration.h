#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Demo
{

enum class EDemoRunMode
{
    InteractiveNative,
    BoundedNative,
    DeterministicHeadless,
    NativeHeadless
};

enum class EDemoGraphicsBackend
{
    Vulkan,
    Metal
};

enum class EDemoWorkload
{
    Triangle,
    ProductionContent
};

enum class EDemoRenderPath
{
    Triangle,
    DeferredFull,
    ForwardSmoke
};

enum class EDemoExitCode : int
{
    Success = 0,
    InvalidConfiguration = 2,
    RuntimeUnavailable = 3,
    InitializationFailed = 4,
    FrameFailed = 5,
    ValidationFailed = 6,
    ReportFailed = 7
};

struct FDemoConfiguration
{
    static constexpr Stoner::Core::uint32 ProductionImageAcceptanceExtent = 256;
    static constexpr Stoner::Core::uint32 ProductionCameraPreviewExtent = 1024;

    EDemoRunMode RunMode = EDemoRunMode::InteractiveNative;
    EDemoGraphicsBackend GraphicsBackend = EDemoGraphicsBackend::Vulkan;
    EDemoWorkload Workload = EDemoWorkload::Triangle;
    EDemoRenderPath RenderPath = EDemoRenderPath::Triangle;
    Stoner::Core::uint32 ClientWidth = 1280;
    Stoner::Core::uint32 ClientHeight = 720;
    Stoner::Core::uint32 FrameBudget = 0;
    Stoner::Core::uint32 WarmupFrames = 0;
    Stoner::Core::uint32 MemorySampleInterval = 0;
    Stoner::Core::uint64 MaxMemoryGrowthBytes = 0;
    double MaxMemoryGrowthPercent = 0.0;
    Stoner::Core::uint32 MaxFramesInFlight = 2;
    Stoner::Core::uint32 ValidationLifecycleCycles = 0;
    bool bEnableValidationLayers = false;
    Stoner::Core::FString ShaderDirectory = "Content/Shaders/Triangle";
    Stoner::Core::FString CookedPublicationRoot;
    Stoner::Core::FString LeaseCoordinationRoot;
    Stoner::Core::FString TargetProfilePath;
    Stoner::Core::FString ProductionRoot;
    Stoner::Core::FString StrictGeneration;
    Stoner::Core::FString WorkloadRevision;
    Stoner::Core::FString BaselineRoot;
    Stoner::Core::FString DeviceClassRegistryPath;
    Stoner::Core::FString ProductionCaptureRoot;
    Stoner::Core::FString ProductionCameraPresetOutput;
    Stoner::Core::uint32 ProductionLifecycleCycles = 20;
    Stoner::Core::uint32 ProductionWarmupCycles = 2;
    Stoner::Core::uint64 ProductionMaxRssGrowthBytes =
        16ULL * 1024ULL * 1024ULL;
    bool bVisibleCapture = false;
    bool bProductionCameraPreview = false;
    Stoner::Core::FString ValidationOutputPath = "Build/triangle-demo-validation.txt";
    Stoner::Core::FString EvidenceRunId = "local";

    [[nodiscard]] bool IsBounded() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetProductionRenderWidth() const noexcept
    {
        return bProductionCameraPreview
            ? ProductionCameraPreviewExtent : ClientWidth;
    }
    [[nodiscard]] Stoner::Core::uint32 GetProductionRenderHeight() const noexcept
    {
        return bProductionCameraPreview
            ? ProductionCameraPreviewExtent : ClientHeight;
    }
    [[nodiscard]] bool RequiresNativeRuntime() const noexcept;
    [[nodiscard]] bool RequiresVisibleWindow() const noexcept;
    [[nodiscard]] bool IsValid(Stoner::Core::FString* OutReason = nullptr) const;

    static EDemoExitCode Parse(int ArgCount, const char* const* Arguments,
        FDemoConfiguration& OutConfiguration, Stoner::Core::FString& OutReason);
};

[[nodiscard]] const char* ToString(EDemoRunMode Mode) noexcept;
[[nodiscard]] const char* ToString(EDemoGraphicsBackend Backend) noexcept;
[[nodiscard]] const char* ToString(EDemoWorkload Workload) noexcept;
[[nodiscard]] const char* ToString(EDemoRenderPath Path) noexcept;

} // namespace Stoner::Demo
