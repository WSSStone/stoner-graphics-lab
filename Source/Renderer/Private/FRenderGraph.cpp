#include "Renderer/FRenderGraph.h"

#include <algorithm>
#include <atomic>
#include <utility>

namespace
{

std::atomic<Stoner::Core::uint32> GNextRenderGraphId{1};

} // namespace

namespace Stoner::Renderer
{

FRenderGraph::FRenderGraph(std::string InName)
    : Name(std::move(InName))
    , GraphId(GNextRenderGraphId.fetch_add(1))
{
}

FRenderGraphBuilder FRenderGraph::CreateBuilder()
{
    return FRenderGraphBuilder(*this);
}

ERenderGraphResult FRenderGraph::Compile()
{
    FRenderGraphCompiler Compiler;
    return Compiler.Compile(*this);
}

ERenderGraphResult FRenderGraph::Execute(const FRenderGraphExecutionDesc& Desc)
{
    FRenderGraphExecutor Executor;
    return Executor.Execute(*this, Desc);
}

void FRenderGraph::Reset()
{
    Passes.clear();
    Resources.clear();
    Outputs.clear();
    ExplicitDependencyEdges.clear();
    CompiledGraph.Clear();
    Diagnostics.Clear();
    State = ERenderGraphState::Draft;
}

void FRenderGraph::Invalidate()
{
    CompiledGraph.Clear();
    Diagnostics.Add(ERenderGraphDiagnosticCategory::Invalidation, ERenderGraphResult::InvalidState, "graph invalidated");
    State = ERenderGraphState::Invalidated;
}

const std::string& FRenderGraph::GetName() const noexcept
{
    return Name;
}

Stoner::Core::uint32 FRenderGraph::GetGraphId() const noexcept
{
    return GraphId;
}

ERenderGraphState FRenderGraph::GetState() const noexcept
{
    return State;
}

const Stoner::Core::TArray<FRenderGraphPassRecord>& FRenderGraph::GetPasses() const noexcept
{
    return Passes;
}

const Stoner::Core::TArray<FRenderGraphResourceRecord>& FRenderGraph::GetResources() const noexcept
{
    return Resources;
}

const Stoner::Core::TArray<FRenderGraphResourceHandle>& FRenderGraph::GetOutputs() const noexcept
{
    return Outputs;
}

const FCompiledRenderGraph& FRenderGraph::GetCompiledGraph() const noexcept
{
    return CompiledGraph;
}

FCompiledRenderGraph& FRenderGraph::GetMutableCompiledGraph() noexcept
{
    return CompiledGraph;
}

const FRenderGraphDiagnosticLog& FRenderGraph::GetDiagnostics() const noexcept
{
    return Diagnostics;
}

std::string FRenderGraph::Dump() const
{
    return CompiledGraph.Dump(*this);
}

const FRenderGraphPassRecord* FRenderGraph::FindPass(FRenderGraphPassHandle Handle) const noexcept
{
    if (!Owns(Handle))
    {
        return nullptr;
    }
    return &Passes[Handle.Index];
}

const FRenderGraphResourceRecord* FRenderGraph::FindResource(FRenderGraphResourceHandle Handle) const noexcept
{
    if (!Owns(Handle))
    {
        return nullptr;
    }
    return &Resources[Handle.Index];
}

FRenderGraphResourceHandle FRenderGraph::AddResource(const FRenderGraphResourceDesc& Desc)
{
    if (State != ERenderGraphState::Draft || !IsValidRenderGraphResourceDesc(Desc))
    {
        Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::ValidationFailed, "invalid resource declaration");
        return {};
    }

    FRenderGraphResourceHandle Handle{GraphId, static_cast<Stoner::Core::uint32>(Resources.size())};
    Resources.push_back({Handle, Desc});
    return Handle;
}

FRenderGraphPassHandle FRenderGraph::AddPass(const FRenderGraphPassDesc& Desc)
{
    if (State != ERenderGraphState::Draft || Desc.Name.empty())
    {
        Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::ValidationFailed, "invalid pass declaration");
        return {};
    }

    for (const FRenderGraphResourceAccess& Access : Desc.Accesses)
    {
        if (!Owns(Access.Resource))
        {
            Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::InvalidHandle, "pass references resource from another graph");
            return {};
        }
    }

    FRenderGraphPassHandle Handle{GraphId, static_cast<Stoner::Core::uint32>(Passes.size())};
    Passes.push_back({Handle, Desc});
    return Handle;
}

ERenderGraphResult FRenderGraph::AddAccess(FRenderGraphPassHandle Pass, FRenderGraphResourceHandle Resource, ERenderGraphAccessType Access)
{
    if (State != ERenderGraphState::Draft)
    {
        return ERenderGraphResult::InvalidState;
    }
    if (!Owns(Pass) || !Owns(Resource))
    {
        Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::InvalidHandle, "access references invalid handle");
        return ERenderGraphResult::InvalidHandle;
    }

    ERenderGraphResourceState RequiredState = ERenderGraphResourceState::Read;
    if (Access == ERenderGraphAccessType::Write || Access == ERenderGraphAccessType::Create)
    {
        RequiredState = ERenderGraphResourceState::Write;
    }
    else if (Access == ERenderGraphAccessType::ReadWrite)
    {
        RequiredState = ERenderGraphResourceState::ReadWrite;
    }
    else if (Access == ERenderGraphAccessType::Import)
    {
        RequiredState = ERenderGraphResourceState::External;
    }
    Passes[Pass.Index].Desc.Accesses.push_back({Resource, Access, RequiredState});
    return ERenderGraphResult::Success;
}

ERenderGraphResult FRenderGraph::AddDependency(FRenderGraphPassHandle Before, FRenderGraphPassHandle After)
{
    if (State != ERenderGraphState::Draft)
    {
        return ERenderGraphResult::InvalidState;
    }
    if (!Owns(Before) || !Owns(After))
    {
        Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::InvalidHandle, "dependency references invalid pass");
        return ERenderGraphResult::InvalidHandle;
    }

    const auto Existing = std::find_if(ExplicitDependencyEdges.begin(), ExplicitDependencyEdges.end(), [Before, After](const FRenderGraphDependencyEdge& Edge) {
        return Edge.FromPassIndex == Before.Index && Edge.ToPassIndex == After.Index;
    });
    if (Existing == ExplicitDependencyEdges.end())
    {
        ExplicitDependencyEdges.push_back({Before.Index, After.Index, {}});
    }
    return ERenderGraphResult::Success;
}

ERenderGraphResult FRenderGraph::AddOutput(FRenderGraphResourceHandle Resource)
{
    if (State != ERenderGraphState::Draft)
    {
        return ERenderGraphResult::InvalidState;
    }
    if (!Owns(Resource))
    {
        Diagnostics.Add(ERenderGraphDiagnosticCategory::Validation, ERenderGraphResult::InvalidHandle, "output references invalid resource");
        return ERenderGraphResult::InvalidHandle;
    }

    const auto Existing = std::find_if(Outputs.begin(), Outputs.end(), [Resource](FRenderGraphResourceHandle ExistingResource) {
        return ExistingResource == Resource;
    });
    if (Existing == Outputs.end())
    {
        Outputs.push_back(Resource);
    }
    return ERenderGraphResult::Success;
}

bool FRenderGraph::Owns(FRenderGraphPassHandle Handle) const noexcept
{
    return Handle.GraphId == GraphId && Handle.Index < Passes.size();
}

bool FRenderGraph::Owns(FRenderGraphResourceHandle Handle) const noexcept
{
    return Handle.GraphId == GraphId && Handle.Index < Resources.size();
}

void FRenderGraph::SetState(ERenderGraphState NewState) noexcept
{
    State = NewState;
}

} // namespace Stoner::Renderer
