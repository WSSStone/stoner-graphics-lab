#pragma once

#include "Core/CoreMinimal.h"

#include <span>

enum class EProductionReadbackPixelFormat
{
    RGBA8UNorm,
    BGRA8UNorm,
    RGBA16Float,
    RGBA32Float,
    R32Float
};

enum class EProductionImageOrigin
{
    TopLeft,
    BottomLeft
};

enum class EProductionColorTransfer
{
    Linear,
    SRGB
};

struct FProductionReadbackView
{
    std::span<const Stoner::Core::uint8> Bytes;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 RowPitchBytes = 0;
    EProductionReadbackPixelFormat Format =
        EProductionReadbackPixelFormat::RGBA8UNorm;
    EProductionImageOrigin Origin = EProductionImageOrigin::TopLeft;
    EProductionColorTransfer Transfer = EProductionColorTransfer::Linear;
};

struct FProductionCanonicalImage
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::TArray<float> LinearRgb;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FProductionRegionProbe
{
    Stoner::Core::FString Name;
    Stoner::Core::uint32 X = 0;
    Stoner::Core::uint32 Y = 0;
    Stoner::Core::FVector3 Expected;
    float Tolerance = 0.0f;
};

struct FProductionSemanticProbeRequest
{
    const FProductionCanonicalImage* Color = nullptr;
    const FProductionCanonicalImage* Normal = nullptr;
    const FProductionCanonicalImage* Depth = nullptr;
    Stoner::Core::uint64 ExpectedFrameToken = 0;
    Stoner::Core::uint64 ObservedFrameToken = 0;
    float MinimumCoverageFraction = 0.01f;
    float MaximumCoverageFraction = 0.99f;
    Stoner::Core::TArray<FProductionRegionProbe> Regions;
    Stoner::Core::TArray<Stoner::Core::FString> RequiredRegionNames;
};

struct FProductionSemanticProbeResult
{
    bool bPassed = false;
    Stoner::Core::FString FirstFailure;
    Stoner::Core::uint32 PassedProbeCount = 0;
};

struct FProductionFlipPolicy
{
    float MeanMax = 0.0f;
    float P95Max = 0.0f;
    float MaximumMax = 0.0f;
    float BadPixelThreshold = 0.0f;
    float BadPixelFractionMax = 0.0f;
};

struct FProductionFlipResult
{
    bool bMeasured = false;
    bool bPassed = false;
    float Mean = 0.0f;
    float P95 = 0.0f;
    float Maximum = 0.0f;
    float BadPixelFraction = 0.0f;
    Stoner::Core::FString FailureReason;
};

struct FProductionNativeImageEvidence
{
    Stoner::Core::FString RequestedBackend;
    Stoner::Core::FString ExecutedBackend;
    Stoner::Core::FString RuntimeMode;
    Stoner::Core::FString WorkloadRevision;
    Stoner::Core::FString BaselineWorkloadRevision;
    bool bNativeExecution = false;
    bool bSubmissionCompleted = false;
    bool bGpuReadback = false;
    bool bPresented = false;
    bool bWindowOnlyCapture = false;
};

[[nodiscard]] bool NormalizeProductionReadback(
    const FProductionReadbackView& Source,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure);

[[nodiscard]] bool NormalizeProductionSignedNormalReadback(
    const FProductionReadbackView& Source,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure);

[[nodiscard]] bool SampleProductionReadbackPixel(
    const FProductionReadbackView& Source,
    Stoner::Core::uint32 X,
    Stoner::Core::uint32 Y,
    Stoner::Core::FVector3& OutValue,
    Stoner::Core::FString& OutFailure);

[[nodiscard]] FProductionSemanticProbeResult RunProductionSemanticProbes(
    const FProductionSemanticProbeRequest& Request);

[[nodiscard]] FProductionFlipResult CompareProductionImagesWithFlip(
    const FProductionCanonicalImage& Reference,
    const FProductionCanonicalImage& Candidate,
    const FProductionFlipPolicy& Policy);

[[nodiscard]] bool ValidateProductionNativeImageEvidence(
    const FProductionNativeImageEvidence& Evidence,
    Stoner::Core::FString& OutFailure);

struct FProductionImageAcceptanceTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FProductionImageAcceptanceTestResult
RunProductionImageAcceptanceTests();
