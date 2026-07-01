#pragma once

#include "Renderer/FRenderGraphBuilder.h"
#include "Renderer/FRenderGraphCompiler.h"
#include "Renderer/FRenderGraphExecutor.h"

#include <string>

namespace Stoner::Renderer
{

class FRenderGraph
{
public:
    explicit FRenderGraph(std::string InName = {});

    [[nodiscard]] FRenderGraphBuilder CreateBuilder();
    [[nodiscard]] ERenderGraphResult Compile();
    [[nodiscard]] ERenderGraphResult Execute(const FRenderGraphExecutionDesc& Desc);
    void Reset();
    void Invalidate();

    [[nodiscard]] const std::string& GetName() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetGraphId() const noexcept;
    [[nodiscard]] ERenderGraphState GetState() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FRenderGraphPassRecord>& GetPasses() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FRenderGraphResourceRecord>& GetResources() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FRenderGraphResourceHandle>& GetOutputs() const noexcept;
    [[nodiscard]] const FCompiledRenderGraph& GetCompiledGraph() const noexcept;
    [[nodiscard]] FCompiledRenderGraph& GetMutableCompiledGraph() noexcept;
    [[nodiscard]] const FRenderGraphDiagnosticLog& GetDiagnostics() const noexcept;
    [[nodiscard]] std::string Dump() const;

    [[nodiscard]] const FRenderGraphPassRecord* FindPass(FRenderGraphPassHandle Handle) const noexcept;
    [[nodiscard]] const FRenderGraphResourceRecord* FindResource(FRenderGraphResourceHandle Handle) const noexcept;

private:
    friend class FRenderGraphBuilder;
    friend class FRenderGraphCompiler;
    friend class FRenderGraphExecutor;

    [[nodiscard]] FRenderGraphResourceHandle AddResource(const FRenderGraphResourceDesc& Desc);
    [[nodiscard]] FRenderGraphPassHandle AddPass(const FRenderGraphPassDesc& Desc);
    [[nodiscard]] ERenderGraphResult AddAccess(FRenderGraphPassHandle Pass, FRenderGraphResourceHandle Resource, ERenderGraphAccessType Access);
    [[nodiscard]] ERenderGraphResult AddDependency(FRenderGraphPassHandle Before, FRenderGraphPassHandle After);
    [[nodiscard]] ERenderGraphResult AddOutput(FRenderGraphResourceHandle Resource);
    [[nodiscard]] bool Owns(FRenderGraphPassHandle Handle) const noexcept;
    [[nodiscard]] bool Owns(FRenderGraphResourceHandle Handle) const noexcept;
    void SetState(ERenderGraphState NewState) noexcept;

    std::string Name;
    Stoner::Core::uint32 GraphId = 0;
    ERenderGraphState State = ERenderGraphState::Draft;
    Stoner::Core::TArray<FRenderGraphPassRecord> Passes;
    Stoner::Core::TArray<FRenderGraphResourceRecord> Resources;
    Stoner::Core::TArray<FRenderGraphResourceHandle> Outputs;
    Stoner::Core::TArray<FRenderGraphDependencyEdge> ExplicitDependencyEdges;
    FCompiledRenderGraph CompiledGraph;
    FRenderGraphDiagnosticLog Diagnostics;
};

} // namespace Stoner::Renderer
