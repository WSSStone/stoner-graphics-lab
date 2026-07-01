#include "Renderer/FRenderGraphCompiler.h"

#include "Renderer/FRenderGraph.h"

#include <algorithm>
#include <deque>
#include <sstream>

namespace
{

using namespace Stoner::Renderer;
using Stoner::Core::uint32;

[[nodiscard]] bool IsCompatibleResourceDescription(const FRenderGraphResourceDesc& A, const FRenderGraphResourceDesc& B)
{
    if (A.Kind != B.Kind)
    {
        return false;
    }
    if (A.Kind == ERenderGraphResourceKind::Texture)
    {
        return A.Width == B.Width && A.Height == B.Height && A.Depth == B.Depth && A.FormatId == B.FormatId;
    }
    return A.SizeInBytes == B.SizeInBytes;
}

[[nodiscard]] ERenderGraphTransitionReason PickTransitionReason(
    ERenderGraphResourceState BeforeState,
    ERenderGraphResourceState AfterState,
    ERenderGraphPassType BeforePass,
    ERenderGraphPassType AfterPass)
{
    if (BeforePass == ERenderGraphPassType::Graphics && AfterPass == ERenderGraphPassType::Compute)
    {
        return ERenderGraphTransitionReason::GraphicsToCompute;
    }
    if (BeforePass == ERenderGraphPassType::Compute && AfterPass == ERenderGraphPassType::Graphics)
    {
        return ERenderGraphTransitionReason::ComputeToGraphics;
    }
    if (BeforeState == ERenderGraphResourceState::Write && AfterState == ERenderGraphResourceState::Read)
    {
        return ERenderGraphTransitionReason::ReadAfterWrite;
    }
    if (BeforeState == ERenderGraphResourceState::Read && AfterState == ERenderGraphResourceState::Write)
    {
        return ERenderGraphTransitionReason::WriteAfterRead;
    }
    return ERenderGraphTransitionReason::WriteAfterWrite;
}

} // namespace

namespace Stoner::Renderer
{

void FCompiledRenderGraph::Clear()
{
    ScheduledPasses.clear();
    CulledPasses.clear();
    DependencyEdges.clear();
    ResourceLifetimes.clear();
    AliasingDecisions.clear();
    TransitionPlan.clear();
    Diagnostics.Clear();
}

bool FCompiledRenderGraph::IsExecutable() const noexcept
{
    return !ScheduledPasses.empty() && !Diagnostics.HasErrors();
}

Stoner::Core::FString FCompiledRenderGraph::Dump(const FRenderGraph& Graph) const
{
    std::ostringstream Stream;
    Stream << "RenderGraph " << Graph.GetName().CStr() << " state=" << ToString(Graph.GetState()) << '\n';

    Stream << "Passes\n";
    for (const FRenderGraphPassRecord& Pass : Graph.GetPasses())
    {
        Stream << "  [" << Pass.Handle.Index << "] " << Pass.Desc.Name.CStr() << " type=" << ToString(Pass.Desc.Type) << " accesses=" << Pass.Desc.Accesses.size() << '\n';
    }

    Stream << "Resources\n";
    for (const FRenderGraphResourceRecord& Resource : Graph.GetResources())
    {
        Stream << "  [" << Resource.Handle.Index << "] " << Resource.Desc.Name.CStr() << " kind=" << ToString(Resource.Desc.Kind) <<
            " ownership=" << ToString(Resource.Desc.Ownership) << " backing=" << Resource.BackingAllocationId << '\n';
    }

    Stream << "Dependencies\n";
    for (const FRenderGraphDependencyEdge& Edge : DependencyEdges)
    {
        Stream << "  " << Edge.FromPassIndex << " -> " << Edge.ToPassIndex << " resource=" << Edge.Resource.Index << '\n';
    }

    Stream << "Schedule\n";
    for (uint32 PassIndex : ScheduledPasses)
    {
        Stream << "  " << PassIndex << '\n';
    }

    Stream << "Culled\n";
    for (uint32 PassIndex : CulledPasses)
    {
        Stream << "  " << PassIndex << '\n';
    }

    Stream << "Lifetimes\n";
    for (const FRenderGraphResourceLifetime& Lifetime : ResourceLifetimes)
    {
        Stream << "  resource=" << Lifetime.Resource.Index << " first=" << Lifetime.FirstUsePassIndex << " last=" << Lifetime.LastUsePassIndex <<
            " imported=" << (Lifetime.bImported ? 1 : 0) << " exported=" << (Lifetime.bExported ? 1 : 0) << '\n';
    }

    Stream << "Aliasing\n";
    for (const FRenderGraphAliasingDecision& Decision : AliasingDecisions)
    {
        Stream << "  " << Decision.FirstResource.Index << "," << Decision.SecondResource.Index << " " <<
            (Decision.State == ERenderGraphAliasDecisionState::Eligible ? "Eligible" : "Rejected") << " reason=" << ToString(Decision.Reason) << '\n';
    }

    Stream << "Transitions\n";
    for (const FRenderGraphTransitionRecord& Transition : TransitionPlan)
    {
        Stream << "  resource=" << Transition.Resource.Index << " before=" << Transition.BeforePassIndex <<
            ':' << ToString(Transition.BeforeState) << " after=" << Transition.AfterPassIndex <<
            ':' << ToString(Transition.AfterState) << " reason=" << ToString(Transition.Reason) << '\n';
    }

    Stream << "Diagnostics\n" << Graph.GetDiagnostics().Format().CStr() << Diagnostics.Format().CStr();
    return Stoner::Core::FString(Stream.str());
}

ERenderGraphResult FRenderGraphCompiler::Compile(FRenderGraph& Graph)
{
    FCompiledRenderGraph& Compiled = Graph.GetMutableCompiledGraph();
    Compiled.Clear();
    Graph.Diagnostics.Clear();

    if (Graph.State != ERenderGraphState::Draft && Graph.State != ERenderGraphState::Failed)
    {
        Graph.Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::InvalidState, "compile requires draft or failed graph");
        Graph.SetState(ERenderGraphState::Failed);
        return ERenderGraphResult::InvalidState;
    }

    const auto& Passes = Graph.GetPasses();
    const auto& Resources = Graph.GetResources();
    const uint32 PassCount = static_cast<uint32>(Passes.size());
    const uint32 ResourceCount = static_cast<uint32>(Resources.size());

    bool bHasSideEffectPass = false;
    for (const FRenderGraphPassRecord& Pass : Passes)
    {
        bHasSideEffectPass = bHasSideEffectPass || Pass.Desc.bPreserveForSideEffects;
    }
    if (Graph.GetOutputs().empty() && !bHasSideEffectPass)
    {
        Graph.Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::ValidationFailed, "graph requires an output or side-effect pass");
        Graph.SetState(ERenderGraphState::Failed);
        return ERenderGraphResult::CompileFailed;
    }

    Stoner::Core::TArray<uint32> LastWriter(ResourceCount, FRenderGraphResourceHandle::InvalidIndex);
    Stoner::Core::TArray<Stoner::Core::TArray<uint32>> LastReaders(ResourceCount);
    Stoner::Core::TArray<Stoner::Core::TArray<uint32>> Adjacency(PassCount);
    Stoner::Core::TArray<uint32> Indegree(PassCount, 0);
    bool bFailed = false;

    auto AddEdge = [&](uint32 From, uint32 To, FRenderGraphResourceHandle Resource) {
        if (From != To && std::find(Adjacency[From].begin(), Adjacency[From].end(), To) == Adjacency[From].end())
        {
            Adjacency[From].push_back(To);
            ++Indegree[To];
            Compiled.DependencyEdges.push_back({From, To, Resource});
        }
    };

    for (const FRenderGraphDependencyEdge& Edge : Graph.ExplicitDependencyEdges)
    {
        AddEdge(Edge.FromPassIndex, Edge.ToPassIndex, Edge.Resource);
    }

    for (const FRenderGraphPassRecord& Pass : Passes)
    {
        for (const FRenderGraphResourceAccess& Access : Pass.Desc.Accesses)
        {
            if (!Graph.Owns(Access.Resource))
            {
                Graph.Diagnostics.AddForPass(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::InvalidHandle, Pass.Handle.Index, "invalid resource access handle");
                bFailed = true;
                continue;
            }

            const FRenderGraphResourceRecord& Resource = Resources[Access.Resource.Index];
            if (Resource.Desc.Ownership == ERenderGraphResourceOwnership::Imported && Resource.Desc.bReadOnlyImported && WritesResource(Access.Access))
            {
                Graph.Diagnostics.AddForResource(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::ValidationFailed, Resource.Handle.Index, "read-only imported resource cannot be written");
                bFailed = true;
            }

            if (ReadsResource(Access.Access) && Resource.Desc.Ownership == ERenderGraphResourceOwnership::Transient &&
                LastWriter[Access.Resource.Index] == FRenderGraphResourceHandle::InvalidIndex)
            {
                Graph.Diagnostics.AddForResource(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::ValidationFailed, Resource.Handle.Index, "transient resource read before write");
                bFailed = true;
            }

            if (LastWriter[Access.Resource.Index] != FRenderGraphResourceHandle::InvalidIndex)
            {
                AddEdge(LastWriter[Access.Resource.Index], Pass.Handle.Index, Access.Resource);
            }

            if (WritesResource(Access.Access))
            {
                for (uint32 Reader : LastReaders[Access.Resource.Index])
                {
                    AddEdge(Reader, Pass.Handle.Index, Access.Resource);
                }
            }

            if (WritesResource(Access.Access))
            {
                LastWriter[Access.Resource.Index] = Pass.Handle.Index;
                LastReaders[Access.Resource.Index].clear();
            }
            if (ReadsResource(Access.Access))
            {
                LastReaders[Access.Resource.Index].push_back(Pass.Handle.Index);
            }
        }
    }

    if (bFailed)
    {
        Graph.SetState(ERenderGraphState::Failed);
        return ERenderGraphResult::CompileFailed;
    }

    std::deque<uint32> Ready;
    for (uint32 PassIndex = 0; PassIndex < PassCount; ++PassIndex)
    {
        if (Indegree[PassIndex] == 0)
        {
            Ready.push_back(PassIndex);
        }
    }

    while (!Ready.empty())
    {
        std::sort(Ready.begin(), Ready.end());
        const uint32 PassIndex = Ready.front();
        Ready.pop_front();
        Compiled.ScheduledPasses.push_back(PassIndex);

        for (uint32 To : Adjacency[PassIndex])
        {
            --Indegree[To];
            if (Indegree[To] == 0)
            {
                Ready.push_back(To);
            }
        }
    }

    if (Compiled.ScheduledPasses.size() != PassCount)
    {
        Graph.Diagnostics.Add(ERenderGraphDiagnosticCategory::Scheduling, ERenderGraphResult::CompileFailed, "dependency cycle detected");
        Graph.SetState(ERenderGraphState::Failed);
        return ERenderGraphResult::CompileFailed;
    }

    Stoner::Core::TArray<bool> Required(PassCount, false);
    for (FRenderGraphResourceHandle Output : Graph.GetOutputs())
    {
        for (const FRenderGraphDependencyEdge& Edge : Compiled.DependencyEdges)
        {
            if (Edge.Resource == Output)
            {
                Required[Edge.FromPassIndex] = true;
                Required[Edge.ToPassIndex] = true;
            }
        }
        if (LastWriter[Output.Index] != FRenderGraphResourceHandle::InvalidIndex)
        {
            Required[LastWriter[Output.Index]] = true;
        }
    }
    for (const FRenderGraphPassRecord& Pass : Passes)
    {
        if (Pass.Desc.bPreserveForSideEffects)
        {
            Required[Pass.Handle.Index] = true;
        }
    }
    bool bChanged = true;
    while (bChanged)
    {
        bChanged = false;
        for (const FRenderGraphDependencyEdge& Edge : Compiled.DependencyEdges)
        {
            if (Required[Edge.ToPassIndex] && !Required[Edge.FromPassIndex])
            {
                Required[Edge.FromPassIndex] = true;
                bChanged = true;
            }
        }
    }

    Stoner::Core::TArray<uint32> CulledSchedule;
    for (uint32 PassIndex : Compiled.ScheduledPasses)
    {
        if (!Required[PassIndex])
        {
            Compiled.CulledPasses.push_back(PassIndex);
        }
        else
        {
            CulledSchedule.push_back(PassIndex);
        }
    }
    Compiled.ScheduledPasses = CulledSchedule;

    for (const FRenderGraphResourceRecord& Resource : Resources)
    {
        FRenderGraphResourceLifetime Lifetime;
        Lifetime.Resource = Resource.Handle;
        Lifetime.bImported = Resource.Desc.Ownership == ERenderGraphResourceOwnership::Imported;
        Lifetime.bExported = Resource.Desc.Ownership == ERenderGraphResourceOwnership::Exported ||
            std::find(Graph.GetOutputs().begin(), Graph.GetOutputs().end(), Resource.Handle) != Graph.GetOutputs().end();

        for (uint32 ScheduleIndex = 0; ScheduleIndex < Compiled.ScheduledPasses.size(); ++ScheduleIndex)
        {
            const uint32 PassIndex = Compiled.ScheduledPasses[ScheduleIndex];
            const FRenderGraphPassRecord& Pass = Passes[PassIndex];
            const bool bUsesResource = std::any_of(Pass.Desc.Accesses.begin(), Pass.Desc.Accesses.end(), [Resource](const FRenderGraphResourceAccess& Access) {
                return Access.Resource == Resource.Handle;
            });
            if (bUsesResource)
            {
                if (Lifetime.FirstUsePassIndex == FRenderGraphResourceHandle::InvalidIndex)
                {
                    Lifetime.FirstUsePassIndex = ScheduleIndex;
                }
                Lifetime.LastUsePassIndex = ScheduleIndex;
            }
        }

        if (Lifetime.FirstUsePassIndex != FRenderGraphResourceHandle::InvalidIndex)
        {
            Compiled.ResourceLifetimes.push_back(Lifetime);
        }
    }

    for (uint32 LeftIndex = 0; LeftIndex < Compiled.ResourceLifetimes.size(); ++LeftIndex)
    {
        for (uint32 RightIndex = LeftIndex + 1; RightIndex < Compiled.ResourceLifetimes.size(); ++RightIndex)
        {
            const FRenderGraphResourceLifetime& LeftLifetime = Compiled.ResourceLifetimes[LeftIndex];
            const FRenderGraphResourceLifetime& RightLifetime = Compiled.ResourceLifetimes[RightIndex];
            const FRenderGraphResourceRecord& LeftResource = Resources[LeftLifetime.Resource.Index];
            const FRenderGraphResourceRecord& RightResource = Resources[RightLifetime.Resource.Index];

            FRenderGraphAliasingDecision Decision;
            Decision.FirstResource = LeftLifetime.Resource;
            Decision.SecondResource = RightLifetime.Resource;
            Decision.State = ERenderGraphAliasDecisionState::Rejected;

            if (LeftResource.Desc.Ownership == ERenderGraphResourceOwnership::Imported || RightResource.Desc.Ownership == ERenderGraphResourceOwnership::Imported)
            {
                Decision.Reason = ERenderGraphAliasReason::ImportedResource;
            }
            else if (LeftLifetime.bExported || RightLifetime.bExported)
            {
                Decision.Reason = ERenderGraphAliasReason::ExportedExternalOwnership;
            }
            else if (LeftResource.Desc.AliasPolicy == ERenderGraphAliasPolicy::Disabled || RightResource.Desc.AliasPolicy == ERenderGraphAliasPolicy::Disabled)
            {
                Decision.Reason = ERenderGraphAliasReason::ExplicitNoAlias;
            }
            else if (!IsCompatibleResourceDescription(LeftResource.Desc, RightResource.Desc))
            {
                Decision.Reason = ERenderGraphAliasReason::IncompatibleDescription;
            }
            else if (LeftLifetime.LastUsePassIndex < RightLifetime.FirstUsePassIndex || RightLifetime.LastUsePassIndex < LeftLifetime.FirstUsePassIndex)
            {
                Decision.State = ERenderGraphAliasDecisionState::Eligible;
                Decision.Reason = ERenderGraphAliasReason::NonOverlappingCompatible;
            }
            else
            {
                Decision.Reason = ERenderGraphAliasReason::OverlappingLifetime;
            }

            Compiled.AliasingDecisions.push_back(Decision);
        }
    }

    Stoner::Core::TArray<ERenderGraphResourceState> CurrentState(ResourceCount, ERenderGraphResourceState::Unknown);
    Stoner::Core::TArray<uint32> LastPassUsingResource(ResourceCount, FRenderGraphPassHandle::InvalidIndex);
    Stoner::Core::TArray<ERenderGraphPassType> LastPassType(ResourceCount, ERenderGraphPassType::Graphics);

    for (uint32 PassIndex : Compiled.ScheduledPasses)
    {
        const FRenderGraphPassRecord& Pass = Passes[PassIndex];
        for (const FRenderGraphResourceAccess& Access : Pass.Desc.Accesses)
        {
            const uint32 ResourceIndex = Access.Resource.Index;
            const ERenderGraphResourceState NextState = Access.RequiredState;
            const bool bWriteAfterWrite =
                LastPassUsingResource[ResourceIndex] != FRenderGraphPassHandle::InvalidIndex &&
                WritesResource(Access.Access) &&
                (CurrentState[ResourceIndex] == ERenderGraphResourceState::Write || CurrentState[ResourceIndex] == ERenderGraphResourceState::ReadWrite);

            if (CurrentState[ResourceIndex] != ERenderGraphResourceState::Unknown && (CurrentState[ResourceIndex] != NextState || bWriteAfterWrite))
            {
                Compiled.TransitionPlan.push_back({
                    Access.Resource,
                    LastPassUsingResource[ResourceIndex],
                    CurrentState[ResourceIndex],
                    PassIndex,
                    NextState,
                    PickTransitionReason(CurrentState[ResourceIndex], NextState, LastPassType[ResourceIndex], Pass.Desc.Type)});
            }
            else if (LastPassUsingResource[ResourceIndex] != FRenderGraphPassHandle::InvalidIndex &&
                LastPassType[ResourceIndex] != Pass.Desc.Type &&
                (LastPassType[ResourceIndex] == ERenderGraphPassType::Graphics || LastPassType[ResourceIndex] == ERenderGraphPassType::Compute) &&
                (Pass.Desc.Type == ERenderGraphPassType::Graphics || Pass.Desc.Type == ERenderGraphPassType::Compute))
            {
                Compiled.TransitionPlan.push_back({
                    Access.Resource,
                    LastPassUsingResource[ResourceIndex],
                    CurrentState[ResourceIndex],
                    PassIndex,
                    NextState,
                    PickTransitionReason(CurrentState[ResourceIndex], NextState, LastPassType[ResourceIndex], Pass.Desc.Type)});
            }
            CurrentState[ResourceIndex] = NextState;
            LastPassUsingResource[ResourceIndex] = PassIndex;
            LastPassType[ResourceIndex] = Pass.Desc.Type;
        }
    }

    Graph.Diagnostics.Add(ERenderGraphDiagnosticCategory::Scheduling, ERenderGraphResult::Success, "graph compiled");
    Graph.SetState(ERenderGraphState::Compiled);
    return ERenderGraphResult::Success;
}

const char* ToString(ERenderGraphTransitionReason Reason) noexcept
{
    switch (Reason)
    {
    case ERenderGraphTransitionReason::ReadAfterWrite: return "ReadAfterWrite";
    case ERenderGraphTransitionReason::WriteAfterRead: return "WriteAfterRead";
    case ERenderGraphTransitionReason::WriteAfterWrite: return "WriteAfterWrite";
    case ERenderGraphTransitionReason::GraphicsToCompute: return "GraphicsToCompute";
    case ERenderGraphTransitionReason::ComputeToGraphics: return "ComputeToGraphics";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
