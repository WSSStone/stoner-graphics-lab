#pragma once

#include "Renderer/FRenderGraphDiagnostics.h"
#include "Renderer/FRenderGraphPass.h"

#include <functional>

namespace Stoner::Renderer
{

class FRenderGraph;

enum class ERenderGraphTransitionReason
{
    ReadAfterWrite,
    WriteAfterRead,
    WriteAfterWrite,
    GraphicsToCompute,
    ComputeToGraphics
};

struct FRenderGraphTransitionRecord
{
    FRenderGraphResourceHandle Resource;
    Stoner::Core::uint32 BeforePassIndex = FRenderGraphPassHandle::InvalidIndex;
    ERenderGraphResourceState BeforeState = ERenderGraphResourceState::Unknown;
    Stoner::Core::uint32 AfterPassIndex = FRenderGraphPassHandle::InvalidIndex;
    ERenderGraphResourceState AfterState = ERenderGraphResourceState::Unknown;
    ERenderGraphTransitionReason Reason = ERenderGraphTransitionReason::ReadAfterWrite;
};

struct FRenderGraphDependencyEdge
{
    Stoner::Core::uint32 FromPassIndex = FRenderGraphPassHandle::InvalidIndex;
    Stoner::Core::uint32 ToPassIndex = FRenderGraphPassHandle::InvalidIndex;
    FRenderGraphResourceHandle Resource;
};

enum class ERenderGraphScheduleEventKind
{
    Transition,
    Pass
};

struct FRenderGraphScheduleEvent
{
    ERenderGraphScheduleEventKind Kind = ERenderGraphScheduleEventKind::Pass;
    Stoner::Core::uint32 PassIndex = FRenderGraphPassHandle::InvalidIndex;
    const FRenderGraphTransitionRecord* Transition = nullptr;
};

using FRenderGraphScheduleVisitor = std::function<ERenderGraphResult(
    const FRenderGraphScheduleEvent&)>;

class FCompiledRenderGraph
{
public:
    void Clear();

    Stoner::Core::TArray<Stoner::Core::uint32> ScheduledPasses;
    Stoner::Core::TArray<Stoner::Core::uint32> CulledPasses;
    Stoner::Core::TArray<FRenderGraphDependencyEdge> DependencyEdges;
    Stoner::Core::TArray<FRenderGraphResourceLifetime> ResourceLifetimes;
    Stoner::Core::TArray<FRenderGraphAliasingDecision> AliasingDecisions;
    Stoner::Core::TArray<FRenderGraphTransitionRecord> TransitionPlan;
    FRenderGraphDiagnosticLog Diagnostics;

    [[nodiscard]] bool IsExecutable() const noexcept;
    [[nodiscard]] ERenderGraphResult VisitSchedule(
        const FRenderGraphScheduleVisitor& Visitor) const;
    [[nodiscard]] Stoner::Core::FString Dump(const FRenderGraph& Graph) const;
};

class FRenderGraphCompiler
{
public:
    [[nodiscard]] ERenderGraphResult Compile(FRenderGraph& Graph);
};

[[nodiscard]] const char* ToString(ERenderGraphTransitionReason Reason) noexcept;

} // namespace Stoner::Renderer
