#pragma once

#include "Renderer/FRenderGraphDiagnostics.h"
#include "Renderer/FRenderGraphPass.h"

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
    [[nodiscard]] Stoner::Core::FString Dump(const FRenderGraph& Graph) const;
};

class FRenderGraphCompiler
{
public:
    [[nodiscard]] ERenderGraphResult Compile(FRenderGraph& Graph);
};

[[nodiscard]] const char* ToString(ERenderGraphTransitionReason Reason) noexcept;

} // namespace Stoner::Renderer
