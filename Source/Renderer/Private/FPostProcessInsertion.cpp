#include "Renderer/FPostProcessInsertion.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace Stoner::Renderer
{
namespace
{

bool ValidToken(const Stoner::Core::FString& Value) noexcept
{
    const std::string_view Text = Value.View();
    return !Text.empty() && Text.size() <= 96 &&
        std::all_of(Text.begin(), Text.end(), [](char Character)
        {
            return (Character >= 'A' && Character <= 'Z') ||
                (Character >= 'a' && Character <= 'z') ||
                (Character >= '0' && Character <= '9') ||
                Character == '.' || Character == '_' || Character == '-';
        });
}

bool UniqueTokens(const Stoner::Core::TArray<Stoner::Core::FString>& Values)
{
    if (std::any_of(Values.begin(), Values.end(),
            [](const auto& Value) { return !ValidToken(Value); }))
        return false;
    std::set<Stoner::Core::FString> Unique(Values.begin(), Values.end());
    return Unique.size() == Values.size();
}

FPostProcessCompositeResolution Failure(
    EPostProcessInsertionResult Result, const char* Reason)
{
    FPostProcessCompositeResolution Out;
    Out.Result = Result;
    Out.StableReason = Reason;
    return Out;
}

bool Contains(const Stoner::Core::TArray<Stoner::Core::FString>& Values,
    const char* Value)
{
    return std::find(Values.begin(), Values.end(), Value) != Values.end();
}

} // namespace

bool FPostProcessOperationDesc::IsStructurallyValid() const noexcept
{
    return ValidToken(OperationId) && ValidToken(StrategyVersion) &&
        DependsOn.size() <= FPostProcessComposite::MaximumOperations &&
        Reads.size() <= FPostProcessComposite::MaximumResourcesPerOperation &&
        Writes.size() <= FPostProcessComposite::MaximumResourcesPerOperation &&
        !Reads.empty() && !Writes.empty() && UniqueTokens(DependsOn) &&
        UniqueTokens(Reads) && UniqueTokens(Writes) &&
        InputDomain != ERenderGraphColorDomain::Unspecified &&
        OutputDomain != ERenderGraphColorDomain::Unspecified;
}

bool FResolvedPostProcessOperation::IsValid() const noexcept
{
    return Declaration.IsStructurallyValid() && ResolvedIndex != 0 &&
        Width != 0 && Height != 0 &&
        SampleCount == Stoner::RHI::ERHISampleCount::One &&
        ColorDomain != ERenderGraphColorDomain::Unspecified &&
        Declaration.InputDomain == ColorDomain &&
        Declaration.OutputDomain == ColorDomain;
}

bool FPostProcessComposite::Add(FPostProcessOperationDesc Operation)
{
    if (Declarations.size() >= MaximumOperations) return false;
    Declarations.push_back(std::move(Operation));
    return true;
}

bool FPostProcessComposite::Add(const IPostProcessStrategy& Strategy)
{
    return Add(Strategy.DescribePostProcessOperation());
}

FPostProcessCompositeResolution FPostProcessComposite::Resolve(
    EPostProcessInsertionPoint ExpectedPoint,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::RHI::ERHISampleCount SampleCount,
    ERenderGraphColorDomain ExpectedDomain) const
{
    if (Declarations.size() > MaximumOperations)
        return Failure(EPostProcessInsertionResult::CapacityExceeded,
            "post-process-capacity-exceeded");
    if (Width == 0 || Height == 0 ||
        SampleCount != Stoner::RHI::ERHISampleCount::One ||
        ExpectedDomain == ERenderGraphColorDomain::Unspecified)
        return Failure(EPostProcessInsertionResult::InvariantViolation,
            "post-process-invalid-inherited-invariants");

    std::map<Stoner::Core::FString, std::size_t> ById;
    std::set<Stoner::Core::int32> OrderKeys;
    for (std::size_t Index = 0; Index < Declarations.size(); ++Index)
    {
        const auto& Operation = Declarations[Index];
        if (!Operation.IsStructurallyValid())
            return Failure(EPostProcessInsertionResult::InvalidOperation,
                "post-process-invalid-operation");
        if (!ById.emplace(Operation.OperationId, Index).second)
            return Failure(EPostProcessInsertionResult::DuplicateOperation,
                "post-process-duplicate-operation");
        if (!OrderKeys.insert(Operation.OrderKey).second)
            return Failure(EPostProcessInsertionResult::DuplicateOrder,
                "post-process-duplicate-order");
        if (Operation.InsertionPoint != ExpectedPoint ||
            Operation.InputDomain != ExpectedDomain ||
            Operation.OutputDomain != ExpectedDomain ||
            !Operation.bPreservesExtent ||
            !Operation.bPreservesSampleCount || Operation.bUsesTemporalState)
            return Failure(EPostProcessInsertionResult::InvariantViolation,
                "post-process-invariant-violation");
        if (Operation.bClaimsToneOrViewingTransform ||
            Operation.bClaimsOutputTransfer || Operation.bClaimsFormalOutput ||
            Operation.bExternalOutput)
            return Failure(EPostProcessInsertionResult::ForbiddenOwnership,
                "post-process-forbidden-ownership");
        if (!Contains(Operation.Reads, "InputColor") ||
            !Contains(Operation.Writes, "OutputColor") ||
            Contains(Operation.Reads, "OutputColor") ||
            Contains(Operation.Writes, "InputColor"))
            return Failure(EPostProcessInsertionResult::UndeclaredHazard,
                "post-process-color-io-contract");
        for (const auto& Read : Operation.Reads)
            if (std::find(Operation.Writes.begin(), Operation.Writes.end(), Read) !=
                    Operation.Writes.end())
                return Failure(EPostProcessInsertionResult::UndeclaredHazard,
                    "post-process-read-write-alias");
    }

    for (const auto& Operation : Declarations)
        for (const auto& Dependency : Operation.DependsOn)
            if (!ById.contains(Dependency))
                return Failure(EPostProcessInsertionResult::MissingDependency,
                    "post-process-missing-dependency");

    enum class EVisit : Stoner::Core::uint8 { Fresh, Active, Complete };
    Stoner::Core::TArray<EVisit> Visits(Declarations.size(), EVisit::Fresh);
    std::function<bool(std::size_t)> Visit = [&](std::size_t Index)
    {
        if (Visits[Index] == EVisit::Active) return false;
        if (Visits[Index] == EVisit::Complete) return true;
        Visits[Index] = EVisit::Active;
        for (const auto& Dependency : Declarations[Index].DependsOn)
            if (!Visit(ById.at(Dependency))) return false;
        Visits[Index] = EVisit::Complete;
        return true;
    };
    for (std::size_t Index = 0; Index < Declarations.size(); ++Index)
        if (!Visit(Index))
            return Failure(EPostProcessInsertionResult::DependencyCycle,
                "post-process-dependency-cycle");

    Stoner::Core::TArray<std::size_t> Order(Declarations.size());
    for (std::size_t Index = 0; Index < Order.size(); ++Index) Order[Index] = Index;
    std::sort(Order.begin(), Order.end(), [&](std::size_t Left, std::size_t Right)
    {
        return Declarations[Left].OrderKey < Declarations[Right].OrderKey;
    });
    for (const std::size_t Index : Order)
        for (const auto& Dependency : Declarations[Index].DependsOn)
            if (Declarations[ById.at(Dependency)].OrderKey >=
                    Declarations[Index].OrderKey)
                return Failure(EPostProcessInsertionResult::DependencyOrder,
                    "post-process-dependency-order");

    std::set<Stoner::Core::FString> ProducedResources;
    FPostProcessCompositeResolution Out;
    Out.Result = EPostProcessInsertionResult::Success;
    Out.StableReason = "post-process-composite-resolved";
    for (std::size_t SortedIndex = 0; SortedIndex < Order.size(); ++SortedIndex)
    {
        const auto& Declaration = Declarations[Order[SortedIndex]];
        for (const auto& Read : Declaration.Reads)
        {
            if (Read != "InputColor" && !ProducedResources.contains(Read))
                return Failure(EPostProcessInsertionResult::ReadBeforeWrite,
                    "post-process-read-before-write");
        }
        for (const auto& Write : Declaration.Writes)
        {
            if (Write != "OutputColor" &&
                !ProducedResources.insert(Write).second)
                return Failure(EPostProcessInsertionResult::DuplicateWriter,
                    "post-process-duplicate-writer");
        }
        FResolvedPostProcessOperation Resolved;
        Resolved.Declaration = Declaration;
        Resolved.ResolvedIndex =
            static_cast<Stoner::Core::uint32>(SortedIndex + 1);
        Resolved.Width = Width;
        Resolved.Height = Height;
        Resolved.SampleCount = SampleCount;
        Resolved.ColorDomain = ExpectedDomain;
        Out.Operations.push_back(std::move(Resolved));
    }
    return Out;
}

bool FOutputTransformDebugBypassRequest::IsValid() const noexcept
{
    if (Mode == EOutputTransformDebugBypassMode::Disabled)
        return StageName.IsEmpty();
    if (!ValidToken(StageName)) return false;
    if (Mode == EOutputTransformDebugBypassMode::HDRPreservingReadback)
        return true;
    return std::isfinite(VisualizationMinimum) &&
        std::isfinite(VisualizationMaximum) &&
        VisualizationMinimum < VisualizationMaximum;
}

bool FResolvedOutputTransformDebugBypass::IsValid() const noexcept
{
    if (Mode == EOutputTransformDebugBypassMode::Disabled)
        return SourceStageId == 0 && SourceStageName.IsEmpty();
    const bool bSceneLinear = SourceDomain ==
        ERenderGraphColorDomain::SceneLinearRec709D65;
    const bool bLuminanceMeaningValid = bSceneLinear
        ? ReferenceWhiteNits == 0.0f && TargetPeakNits == 0.0f
        : std::isfinite(ReferenceWhiteNits) && ReferenceWhiteNits > 0.0f &&
            std::isfinite(TargetPeakNits) &&
            TargetPeakNits >= ReferenceWhiteNits;
    return SourceStageId != 0 && ValidToken(SourceStageName) &&
        SourceDomain != ERenderGraphColorDomain::Unspecified &&
        bLuminanceMeaningValid && bNonAuthoritative &&
        (Mode == EOutputTransformDebugBypassMode::HDRPreservingReadback ||
            (std::isfinite(VisualizationMinimum) &&
             std::isfinite(VisualizationMaximum) &&
             VisualizationMinimum < VisualizationMaximum));
}

const char* ToString(EPostProcessInsertionPoint Point) noexcept
{
    return Point == EPostProcessInsertionPoint::PreTonemap
        ? "PreTonemap" : "PostTonemap";
}

const char* ToString(EPostProcessInsertionResult Result) noexcept
{
    switch (Result)
    {
    case EPostProcessInsertionResult::Success: return "Success";
    case EPostProcessInsertionResult::CapacityExceeded: return "CapacityExceeded";
    case EPostProcessInsertionResult::InvalidOperation: return "InvalidOperation";
    case EPostProcessInsertionResult::DuplicateOperation: return "DuplicateOperation";
    case EPostProcessInsertionResult::DuplicateOrder: return "DuplicateOrder";
    case EPostProcessInsertionResult::MissingDependency: return "MissingDependency";
    case EPostProcessInsertionResult::DependencyCycle: return "DependencyCycle";
    case EPostProcessInsertionResult::DependencyOrder: return "DependencyOrder";
    case EPostProcessInsertionResult::ReadBeforeWrite: return "ReadBeforeWrite";
    case EPostProcessInsertionResult::DuplicateWriter: return "DuplicateWriter";
    case EPostProcessInsertionResult::UndeclaredHazard: return "UndeclaredHazard";
    case EPostProcessInsertionResult::InvariantViolation: return "InvariantViolation";
    case EPostProcessInsertionResult::ForbiddenOwnership: return "ForbiddenOwnership";
    }
    return "Unknown";
}

const char* ToString(EOutputTransformDebugBypassMode Mode) noexcept
{
    switch (Mode)
    {
    case EOutputTransformDebugBypassMode::Disabled: return "Disabled";
    case EOutputTransformDebugBypassMode::HDRPreservingReadback:
        return "HDRPreservingReadback";
    case EOutputTransformDebugBypassMode::BoundedVisualization:
        return "BoundedVisualization";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
