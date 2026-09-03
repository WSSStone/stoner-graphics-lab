#pragma once

#include "Core/CoreMinimal.h"

#include <optional>
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

struct FProductionPixelRegion
{
    Stoner::Core::uint32 MinimumX = 0;
    Stoner::Core::uint32 MinimumY = 0;
    Stoner::Core::uint32 MaximumXExclusive = 0;
    Stoner::Core::uint32 MaximumYExclusive = 0;

    [[nodiscard]] bool IsValid(
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height) const noexcept;
};

enum class EProductionRegionStatistic
{
    Median,
    Quantile
};

struct FProductionRegionProbe
{
    Stoner::Core::FString Name;
    FProductionPixelRegion Region;
    Stoner::Core::FVector3 Expected;
    float Tolerance = 0.0f;
    float MinimumValidSampleFraction = 0.5f;
    EProductionRegionStatistic Statistic = EProductionRegionStatistic::Median;
    float Quantile = 0.5f;
};

struct FProductionReadbackRegionSample
{
    Stoner::Core::FVector3 Value;
    float ValidSampleFraction = 0.0f;
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
    Stoner::Core::TArray<Stoner::Core::FString> PassedProbeIds;
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

struct FOutputTransformSdrAcceptanceV3
{
    Stoner::Core::FString MaintainerId;
    Stoner::Core::FString ReviewedAt;
    Stoner::Core::FString CandidateSha256;
    Stoner::Core::FString Decision;
};

struct FOutputTransformSdrBaselineV3
{
    Stoner::Core::FString BaselineId;
    Stoner::Core::FString State;
    Stoner::Core::FString WorkloadRevision;
    Stoner::Core::FString Backend;
    Stoner::Core::FString DeviceClass;
    Stoner::Core::FString CapabilityDigest;
    Stoner::Core::FString OutputDeviceProfileId;
    Stoner::Core::FString TransformVersion;
    double ExposureStops = 0.0;
    Stoner::Core::FString SettingsDigest;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 SampleCount = 0;
    Stoner::Core::FString ReferencePath;
    Stoner::Core::FString CompressedSha256;
    Stoner::Core::FString DecodedSha256;
    Stoner::Core::FString CalibrationEvidenceSha256;
    FProductionFlipPolicy FlipPolicy;
    std::optional<FOutputTransformSdrAcceptanceV3> Acceptance;
};

class FOutputTransformSdrBaselineRegistryV3
{
public:
    [[nodiscard]] bool LoadRegistry(
        const Stoner::Core::FString& Path,
        Stoner::Core::FString& OutFailure);

    [[nodiscard]] bool SelectAccepted(
        const Stoner::Core::FString& WorkloadRevision,
        const Stoner::Core::FString& Backend,
        const Stoner::Core::FString& DeviceClass,
        const Stoner::Core::FString& OutputDeviceProfileId,
        const Stoner::Core::FString& TransformVersion,
        double ExposureStops,
        const Stoner::Core::FString& SettingsDigest,
        FOutputTransformSdrBaselineV3& OutBaseline,
        Stoner::Core::FString& OutFailure) const;

    [[nodiscard]] static bool IsAllowedStateTransition(
        const Stoner::Core::FString& From,
        const Stoner::Core::FString& To) noexcept;

private:
    Stoner::Core::TArray<FOutputTransformSdrBaselineV3> Records;
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

[[nodiscard]] bool SampleProductionReadbackRegion(
    const FProductionReadbackView& Source,
    const FProductionPixelRegion& Region,
    float Quantile,
    FProductionReadbackRegionSample& OutSample,
    Stoner::Core::FString& OutFailure);

[[nodiscard]] bool MeasureProductionReadbackDirectionalCoverage(
    const FProductionReadbackView& Source,
    const FProductionPixelRegion& Region,
    const Stoner::Core::FVector3& ExpectedDirection,
    float MinimumDot,
    float& OutCoverage,
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
