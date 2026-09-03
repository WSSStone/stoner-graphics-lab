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

    Stoner::Core::TArray<bool> RequiredResources(Graph.Resources.size(), false);
    for (Stoner::Core::uint32 PassIndex : Graph.GetCompiledGraph().ScheduledPasses)
    {
        const FRenderGraphPassRecord& Pass = Graph.Passes[PassIndex];
        for (const FRenderGraphResourceAccess& Access : Pass.Desc.Accesses)
        {
            if (Access.Resource.Index < RequiredResources.size())
            {
                RequiredResources[Access.Resource.Index] = true;
            }
        }
    }

    Stoner::Core::TArray<FRenderGraphResolvedResource> ResolvedResources;
    Stoner::Core::uint32 NextBackingId = 1;
    for (FRenderGraphResourceRecord& Resource : Graph.Resources)
    {
        if (!RequiredResources[Resource.Handle.Index])
        {
            Resource.BackingAllocationId = 0;
            Resource.bResolvedDuringExecution = false;
            continue;
        }

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

    const ERenderGraphResult ScheduleResult = Graph.GetCompiledGraph().VisitSchedule(
        [&](const FRenderGraphScheduleEvent& Event) {
            if (Event.Kind == ERenderGraphScheduleEventKind::Transition)
            {
                const ERenderGraphResult TransitionResult =
                    CommandContext.EmitTransition(*Event.Transition);
                if (TransitionResult != ERenderGraphResult::Success)
                {
                    Graph.Diagnostics.AddForResource(
                        ERenderGraphDiagnosticCategory::Execution,
                        TransitionResult,
                        Event.Transition->Resource.Index,
                        "transition emission failed");
                    return TransitionResult;
                }
            }
            else
            {
                FRenderGraphPassRecord& Pass = Graph.Passes[Event.PassIndex];
                if (Pass.Desc.Callback)
                {
                    FRenderGraphExecutionContext Context(
                        Graph, Pass, ResolvedResources, CommandContext);
                    const ERenderGraphResult PassResult = Pass.Desc.Callback(Context);
                    if (PassResult != ERenderGraphResult::Success)
                    {
                        Graph.Diagnostics.AddForPass(
                            ERenderGraphDiagnosticCategory::Execution,
                            PassResult,
                            Event.PassIndex,
                            "pass execution failed: " + Pass.Desc.Name.ToStdString());
                        return PassResult;
                    }
                }
            }
            return Desc.ScheduleVisitor ? Desc.ScheduleVisitor(Event) :
                                          ERenderGraphResult::Success;
        });
    if (ScheduleResult != ERenderGraphResult::Success)
    {
        Graph.SetState(ERenderGraphState::Failed);
        return ScheduleResult;
    }

    Graph.Diagnostics.Add(ERenderGraphDiagnosticCategory::Execution, ERenderGraphResult::Success, "graph executed");
    Graph.SetState(ERenderGraphState::Executed);
    return ERenderGraphResult::Success;
}

} // namespace Stoner::Renderer
