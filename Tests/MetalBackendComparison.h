#pragma once

#include "Core/CoreMinimal.h"

enum class EMetalReadbackFormat
{
    RGBA8UNorm,
    RGBA16Float,
    R32Float
};

enum class EMetalReadbackOrigin
{
    TopLeft,
    BottomLeft
};

enum class EMetalReadbackColorSpace
{
    Linear,
    SRGB
};

enum class EMetalReadbackDepthConvention
{
    StandardZ,
    ReversedZ
};

enum class EMetalReadbackSemantic
{
    FinalLdrColor,
    LinearHdrColor,
    ScalarLighting,
    NormalizedDepth,
    WorldNormal,
    Metallic,
    Roughness,
    AmbientOcclusion
};

struct FMetalBackendReadback
{
    Stoner::Core::FString Backend;
    Stoner::Core::FString EvidenceReference;
    Stoner::Core::FString WorkloadIdentity;
    Stoner::Core::FString ShaderVersion;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 RowPitchBytes = 0;
    EMetalReadbackFormat Format = EMetalReadbackFormat::RGBA8UNorm;
    EMetalReadbackOrigin Origin = EMetalReadbackOrigin::TopLeft;
    EMetalReadbackColorSpace ColorSpace = EMetalReadbackColorSpace::Linear;
    EMetalReadbackDepthConvention DepthConvention =
        EMetalReadbackDepthConvention::StandardZ;
    EMetalReadbackSemantic Semantic = EMetalReadbackSemantic::FinalLdrColor;
    Stoner::Core::uint32 ScalarChannel = 0;
    bool bNormalEncodedUNorm = false;
    bool bWholeImage = false;
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes;
};

struct FMetalBackendComparisonReport
{
    static constexpr const char* ToleranceSet =
        "metal-vulkan-tolerance-v1";

    bool bPassed = false;
    Stoner::Core::uint64 ComparedPixelCount = 0;
    Stoner::Core::uint64 WithinPrimaryTolerancePixelCount = 0;
    double WithinPrimaryToleranceRatio = 0.0;
    double MaximumAbsoluteError = 0.0;
    double MinimumNormalDot = 1.0;
    Stoner::Core::FString LeftBackend;
    Stoner::Core::FString RightBackend;
    Stoner::Core::FString LeftEvidenceReference;
    Stoner::Core::FString RightEvidenceReference;
    Stoner::Core::FString WorkloadIdentity;
    Stoner::Core::FString ShaderVersion;
    Stoner::Core::FString FailureCode;
    Stoner::Core::FString FailureReason;

    [[nodiscard]] Stoner::Core::FString Dump() const;
};

[[nodiscard]] FMetalBackendComparisonReport CompareMetalBackendReadbacks(
    const FMetalBackendReadback& Left,
    const FMetalBackendReadback& Right);
