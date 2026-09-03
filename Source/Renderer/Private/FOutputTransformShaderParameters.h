#pragma once

#include "FOutputTransformReference.h"
#include "Renderer/FOutputTransformSettings.h"

#include <cmath>

namespace Stoner::Renderer::Private
{

enum class EOutputTransformShaderStageMode : Stoner::Core::uint32
{
    ManualExposure = 0,
    ToneOrViewing = 1,
    OutputDevice = 2
};

struct alignas(16) FOutputTransformShaderParameters
{
    float ExposureScale = 1.0f;
    float ReferenceWhiteNits = 100.0f;
    float TargetPeakNits = 100.0f;
    float Reserved0 = 0.0f;
    Stoner::Core::uint32 StageMode = 0;
    Stoner::Core::uint32 TransformStrategy = 0;
    Stoner::Core::uint32 OutputProfile = 0;
    Stoner::Core::uint32 NativeEncoding = 0;
};

static_assert(sizeof(FOutputTransformShaderParameters) == 32);

struct FOutputTransformShaderParameterBinding
{
    FOutputTransformShaderParameters Parameters;
    Stoner::Core::FString PipelineKey;
    Stoner::Core::FString ProfileRegistryDigest;
    Stoner::Core::FString ReferenceVectorSetDigest;
    Stoner::Core::FString TransformConstantsDigest;
    Stoner::Core::FString TolerancePolicyDigest;
    Stoner::Core::uint32 ExposureApplicationCount = 0;

    [[nodiscard]] bool IsValid() const noexcept;
};

class FOutputTransformShaderParameterBuilder final
{
public:
    [[nodiscard]] static FOutputTransformShaderParameterBinding Build(
        const FResolvedOutputTransformSettings& Settings,
        EOutputTransformShaderStageMode Stage) noexcept;
};

struct FFrozenOutputTransformConformanceAuthority
{
    Stoner::Core::FString ProfileRegistryDigest;
    Stoner::Core::FString VectorManifestDigest;
    Stoner::Core::FString VectorSetDigest;
    Stoner::Core::uint32 CaseCount = 0;
    Stoner::Core::uint32 ExpectationCount = 0;
    bool bCanGenerateExpectedValues = true;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FOutputTransformConformanceReport
{
    bool bSucceeded = false;
    bool bComparedDecodedDomain = false;
    bool bComparedRawLinearHdrCodes = false;
    EOutputComparisonDomain ComparisonDomain =
        EOutputComparisonDomain::LinearRec709;
    Stoner::RHI::ERHIPresentationNativeEncoding StorageEncoding =
        Stoner::RHI::ERHIPresentationNativeEncoding::Unknown;
    FOutputTransformReferenceRgb Encoded;
    FOutputTransformReferenceRgb Decoded;
    FOutputTransformReferenceRgb DecodedTolerance;
    FOutputTransformReferenceXyz ExpectedXyz;

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] FFrozenOutputTransformConformanceAuthority
LoadFrozenOutputTransformConformanceAuthority();

[[nodiscard]] FOutputTransformConformanceReport
EvaluateOutputTransformConformance(
    const FResolvedOutputTransformSettings& Settings,
    FOutputTransformReferenceRgb SceneLinear) noexcept;

} // namespace Stoner::Renderer::Private
