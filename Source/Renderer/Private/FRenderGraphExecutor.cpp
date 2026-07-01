#include "Renderer/FRenderGraphExecutor.h"

#include "Renderer/FRenderGraph.h"

#include <algorithm>

namespace Stoner::Renderer
{

ERenderGraphResult FRenderGraphCommandContext::EmitTransition(const FRenderGraphTransitionRecord& Transition)
{
    if (bFailTransitionEmission)
    {
        return ERenderGraphResult::ExecutionFailed;
    }
    EmittedTransitions.push_back(Transition);
    return ERenderGraphResult::Success;
}

FRenderGraphExecutionContext::FRenderGraphExecutionContext(
    const FRenderGraph& InGraph,
    const FRenderGraphPassRecord& InPass,
    const Stoner::Core::TArray<FRenderGraphResolvedResource>& InResources,
    FRenderGraphCommandContext& InCommandContext)
    : Graph(InGraph)
    , Pass(InPass)
    , Resources(InResources)
    , CommandContext(InCommandContext)
{
}

const FRenderGraphPassRecord& FRenderGraphExecutionContext::GetPass() const noexcept
{
    return Pass;
}

bool FRenderGraphExecutionContext::CanAccess(FRenderGraphResourceHandle Handle) const noexcept
{
    (void)Graph;
    return std::any_of(Pass.Desc.Accesses.begin(), Pass.Desc.Accesses.end(), [Handle](const FRenderGraphResourceAccess& Access) {
        return Access.Resource == Handle;
    });
}

const FRenderGraphResolvedResource* FRenderGraphExecutionContext::FindResource(FRenderGraphResourceHandle Handle) const noexcept
{
    if (!CanAccess(Handle))
    {
        return nullptr;
    }
    const auto It = std::find_if(Resources.begin(), Resources.end(), [Handle](const FRenderGraphResolvedResource& Resource) {
        return Resource.Handle == Handle;
    });
    return It == Resources.end() ? nullptr : &(*It);
}

FRenderGraphCommandContext& FRenderGraphExecutionContext::GetCommandContext() noexcept
{
    return CommandContext;
}

ERenderGraphResult FRenderGraphExecutor::Execute(FRenderGraph& Graph, const FRenderGraphExecutionDesc& Desc)
{
    if (Graph.GetState() != ERenderGraphState::Compiled || !Graph.GetCompiledGraph().IsExecutable())
    {
        Graph.Diagnostics.Add(ERenderGraphDiagnosticCategory::Execution, ERenderGraphResult::InvalidState, "execution requires compiled graph");
        Graph.SetState(ERenderGraphState::Failed);
        return ERenderGraphResult::InvalidState;
    }

    FRenderGraphCommandContext LocalCommandContext;
    FRenderGraphCommandContext& CommandContext = Desc.CommandContext ? *Desc.CommandContext : LocalCommandContext;

    Stoner::Core::TArray<FRenderGraphResolvedResource> ResolvedResources;
    Stoner::Core::uint32 NextBackingId = 1;
    for (FRenderGraphResourceRecord& Resource : Graph.Resources)
    {
        if (Resource.Desc.Ownership == ERenderGraphResourceOwnership::Imported)
        {
            const auto Binding = std::find_if(Desc.ImportedResources.begin(), Desc.ImportedResources.end(), [Resource](const FRenderGraphImportedResourceBinding& Candidate) {
                return Candidate.Handle == Resource.Handle && Candidate.ExternalToken != 0;
            });
            if (Binding == Desc.ImportedResources.end())
            {
                Graph.Diagnostics.AddForResource(ERenderGraphDiagnosticCategory::Execution, ERenderGraphResult::ResourceUnavailable, Resource.Handle.Index, "missing imported resource");
                Graph.SetState(ERenderGraphState::Failed);
                return ERenderGraphResult::ResourceUnavailable;
            }
            Resource.BackingAllocationId = static_cast<Stoner::Core::uint32>(Binding->ExternalToken);
            Resource.bResolvedDuringExecution = true;
            ResolvedResources.push_back({Resource.Handle, Resource.BackingAllocationId, true});
        }
        else
        {
            if (Desc.bFailTransientResolution)
            {
                Graph.Diagnostics.AddForResource(ERenderGraphDiagnosticCategory::Execution, ERenderGraphResult::ResourceUnavailable, Resource.Handle.Index, "transient resource resolution failed");
                Graph.SetState(ERenderGraphState::Failed);
                return ERenderGraphResult::ResourceUnavailable;
            }
            Resource.BackingAllocationId = NextBackingId++;
            Resource.bResolvedDuringExecution = true;
            ResolvedResources.push_back({Resource.Handle, Resource.BackingAllocationId, false});
        }
    }

    // Foundation-phase execution emits the entire compiled transition plan up front,
    // before any pass callback runs. This is valid here because passes perform no real
    // GPU work and the plan order is preserved for inspection. When a real backend records
    // commands, transitions must instead be interleaved per pass (emitted immediately
    // before the scheduled pass that requires the new state) — see US4-AC2.
    for (const FRenderGraphTransitionRecord& Transition : Graph.GetCompiledGraph().TransitionPlan)
    {
        const ERenderGraphResult TransitionResult = CommandContext.EmitTransition(Transition);
        if (TransitionResult != ERenderGraphResult::Success)
        {
            Graph.Diagnostics.AddForResource(ERenderGraphDiagnosticCategory::Execution, TransitionResult, Transition.Resource.Index, "transition emission failed");
            Graph.SetState(ERenderGraphState::Failed);
            return TransitionResult;
        }
    }

    for (Stoner::Core::uint32 PassIndex : Graph.GetCompiledGraph().ScheduledPasses)
    {
        FRenderGraphPassRecord& Pass = Graph.Passes[PassIndex];
        if (!Pass.Desc.Callback)
        {
            continue;
        }

        FRenderGraphExecutionContext Context(Graph, Pass, ResolvedResources, CommandContext);
        const ERenderGraphResult PassResult = Pass.Desc.Callback(Context);
        if (PassResult != ERenderGraphResult::Success)
        {
            Graph.Diagnostics.AddForPass(ERenderGraphDiagnosticCategory::Execution, PassResult, PassIndex, "pass execution failed: " + Pass.Desc.Name.ToStdString());
            Graph.SetState(ERenderGraphState::Failed);
            return PassResult;
        }
    }

    Graph.Diagnostics.Add(ERenderGraphDiagnosticCategory::Execution, ERenderGraphResult::Success, "graph executed");
    Graph.SetState(ERenderGraphState::Executed);
    return ERenderGraphResult::Success;
}

} // namespace Stoner::Renderer
