#pragma once

#include "Renderer/FRenderGraphCompiler.h"

namespace Stoner::Renderer
{

class FRenderGraph;

struct FRenderGraphResolvedResource
{
    FRenderGraphResourceHandle Handle;
    Stoner::Core::uint32 BackingAllocationId = 0;
    bool bImported = false;
};

struct FRenderGraphImportedResourceBinding
{
    FRenderGraphResourceHandle Handle;
    Stoner::Core::uint64 ExternalToken = 0;
};

class FRenderGraphCommandContext
{
public:
    bool bFailTransitionEmission = false;
    Stoner::Core::TArray<FRenderGraphTransitionRecord> EmittedTransitions;

    [[nodiscard]] ERenderGraphResult EmitTransition(const FRenderGraphTransitionRecord& Transition);
};

struct FRenderGraphExecutionDesc
{
    Stoner::Core::TArray<FRenderGraphImportedResourceBinding> ImportedResources;
    FRenderGraphCommandContext* CommandContext = nullptr;
    bool bFailTransientResolution = false;
};

class FRenderGraphExecutionContext
{
public:
    FRenderGraphExecutionContext(
        const FRenderGraph& InGraph,
        const FRenderGraphPassRecord& InPass,
        const Stoner::Core::TArray<FRenderGraphResolvedResource>& InResources,
        FRenderGraphCommandContext& InCommandContext);

    [[nodiscard]] const FRenderGraphPassRecord& GetPass() const noexcept;
    [[nodiscard]] bool CanAccess(FRenderGraphResourceHandle Handle) const noexcept;
    [[nodiscard]] const FRenderGraphResolvedResource* FindResource(FRenderGraphResourceHandle Handle) const noexcept;
    [[nodiscard]] FRenderGraphCommandContext& GetCommandContext() noexcept;

private:
    const FRenderGraph& Graph;
    const FRenderGraphPassRecord& Pass;
    const Stoner::Core::TArray<FRenderGraphResolvedResource>& Resources;
    FRenderGraphCommandContext& CommandContext;
};

class FRenderGraphExecutor
{
public:
    [[nodiscard]] ERenderGraphResult Execute(FRenderGraph& Graph, const FRenderGraphExecutionDesc& Desc);
};

} // namespace Stoner::Renderer
