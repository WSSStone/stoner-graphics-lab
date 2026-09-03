#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FRenderGraphResource.h"

namespace Stoner::Renderer
{

enum class EPostProcessInsertionPoint
{
    PreTonemap,
    PostTonemap
};

enum class EPostProcessInsertionResult
{
    Success,
    CapacityExceeded,
    InvalidOperation,
    DuplicateOperation,
    DuplicateOrder,
    MissingDependency,
    DependencyCycle,
    DependencyOrder,
    ReadBeforeWrite,
    DuplicateWriter,
    UndeclaredHazard,
    InvariantViolation,
    ForbiddenOwnership
};

struct FPostProcessOperationDesc
{
    Stoner::Core::FString OperationId;
    Stoner::Core::FString StrategyVersion;
    EPostProcessInsertionPoint InsertionPoint =
        EPostProcessInsertionPoint::PreTonemap;
    Stoner::Core::int32 OrderKey = 0;
    Stoner::Core::TArray<Stoner::Core::FString> DependsOn;
    Stoner::Core::TArray<Stoner::Core::FString> Reads = {"InputColor"};
    Stoner::Core::TArray<Stoner::Core::FString> Writes = {"OutputColor"};
    ERenderGraphColorDomain InputDomain =
        ERenderGraphColorDomain::Unspecified;
    ERenderGraphColorDomain OutputDomain =
        ERenderGraphColorDomain::Unspecified;
    bool bPreservesExtent = true;
    bool bPreservesSampleCount = true;
    bool bClaimsToneOrViewingTransform = false;
    bool bClaimsOutputTransfer = false;
    bool bClaimsFormalOutput = false;
    bool bUsesTemporalState = false;
    bool bExternalOutput = false;

    [[nodiscard]] bool IsStructurallyValid() const noexcept;
};

class IPostProcessStrategy
{
public:
    virtual ~IPostProcessStrategy() = default;
    [[nodiscard]] virtual const FPostProcessOperationDesc&
        DescribePostProcessOperation() const noexcept = 0;
};

struct FResolvedPostProcessOperation
{
    FPostProcessOperationDesc Declaration;
    Stoner::Core::uint32 ResolvedIndex = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::RHI::ERHISampleCount SampleCount =
        Stoner::RHI::ERHISampleCount::One;
    ERenderGraphColorDomain ColorDomain =
        ERenderGraphColorDomain::Unspecified;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FPostProcessCompositeResolution
{
    EPostProcessInsertionResult Result =
        EPostProcessInsertionResult::InvalidOperation;
    Stoner::Core::TArray<FResolvedPostProcessOperation> Operations;
    Stoner::Core::FString StableReason;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == EPostProcessInsertionResult::Success;
    }
};

class FPostProcessComposite
{
public:
    static constexpr Stoner::Core::uint32 MaximumOperations = 16;
    static constexpr Stoner::Core::uint32 MaximumResourcesPerOperation = 8;

    [[nodiscard]] bool Add(FPostProcessOperationDesc Operation);
    [[nodiscard]] bool Add(const IPostProcessStrategy& Strategy);
    [[nodiscard]] const Stoner::Core::TArray<FPostProcessOperationDesc>&
        GetDeclarations() const noexcept { return Declarations; }
    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return Declarations.empty();
    }
    [[nodiscard]] FPostProcessCompositeResolution Resolve(
        EPostProcessInsertionPoint ExpectedPoint,
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height,
        Stoner::RHI::ERHISampleCount SampleCount,
        ERenderGraphColorDomain ExpectedDomain) const;

private:
    Stoner::Core::TArray<FPostProcessOperationDesc> Declarations;
};

enum class EOutputTransformDebugBypassMode
{
    Disabled,
    HDRPreservingReadback,
    BoundedVisualization
};

struct FOutputTransformDebugBypassRequest
{
    Stoner::Core::FString StageName;
    EOutputTransformDebugBypassMode Mode =
        EOutputTransformDebugBypassMode::Disabled;
    float VisualizationMinimum = 0.0f;
    float VisualizationMaximum = 1.0f;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FResolvedOutputTransformDebugBypass
{
    Stoner::Core::uint32 SourceStageId = 0;
    Stoner::Core::FString SourceStageName;
    EOutputTransformDebugBypassMode Mode =
        EOutputTransformDebugBypassMode::Disabled;
    ERenderGraphColorDomain SourceDomain =
        ERenderGraphColorDomain::Unspecified;
    float ReferenceWhiteNits = 0.0f;
    float TargetPeakNits = 0.0f;
    float VisualizationMinimum = 0.0f;
    float VisualizationMaximum = 1.0f;
    bool bNonAuthoritative = true;

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] const char* ToString(
    EPostProcessInsertionPoint Point) noexcept;
[[nodiscard]] const char* ToString(
    EPostProcessInsertionResult Result) noexcept;
[[nodiscard]] const char* ToString(
    EOutputTransformDebugBypassMode Mode) noexcept;

} // namespace Stoner::Renderer
