#include "Renderer/FDeferredRenderGraphDeclaration.h"

#include <sstream>

namespace Stoner::Renderer
{

const FDeferredGraphResource* FDeferredRenderGraphDeclaration::FindResource(
    const Stoner::Core::FString& Name) const noexcept
{
    for (const FDeferredGraphResource& Resource : Resources)
    {
        if (Resource.Name == Name)
        {
            return &Resource;
        }
    }
    return nullptr;
}

Stoner::Core::FString FDeferredRenderGraphDeclaration::Dump() const
{
    std::ostringstream Stream;
    Stream << "DeferredGraph valid=" << (bValid ? 1 : 0)
        << " sceneColor=" << SceneColorOutput.CStr()
        << " formalOutput=" << FinalOutput.CStr() << '\n';
    for (const FDeferredGraphResource& Resource : Resources)
    {
        Stream << "Resource " << Resource.Name.CStr() << " format="
            << static_cast<int>(Resource.Format) << " ownership="
            << ToString(Resource.Ownership) << '\n';
    }
    for (const FDeferredGraphPass& Pass : Passes)
    {
        Stream << "Pass " << Pass.Name.CStr() << " stage=" << ToString(Pass.Stage)
            << " culled=" << (Pass.bCulled ? 1 : 0) << '\n';
    }
    for (const FDeferredGraphAccess& Access : Accesses)
    {
        Stream << "Access " << Access.PassName.CStr() << ' ' << Access.ResourceName.CStr()
            << ' ' << ToString(Access.Access) << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

FDeferredRenderGraphDeclaration BuildDeferredRenderGraphDeclaration(
    const FDeferredFramePlan& Plan, FDeferredDiagnosticLog* Diagnostics)
{
    FDeferredRenderGraphDeclaration Graph;
    if (!Plan.IsValid())
    {
        if (Diagnostics)
        {
            Diagnostics->Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::SurfaceData,
                EDeferredResult::GraphCompilationFailed, "DEF-GRAPH-PLAN", Plan.FrameId,
                "invalid frame plan cannot declare a render graph");
        }
        return Graph;
    }

    const FDeferredExtent2D Extent = Plan.SurfaceLayout.Extent;
    for (const FDeferredSurfaceAttachment& Attachment : Plan.SurfaceLayout.Attachments)
    {
        Graph.Resources.push_back({Attachment.Name, ERenderGraphResourceKind::Texture,
            ERenderGraphResourceOwnership::Transient, Attachment.Format, Extent});
    }
    Graph.Resources.push_back({"LightingAccumulation", ERenderGraphResourceKind::Texture,
        ERenderGraphResourceOwnership::Transient, Stoner::RHI::ERHIFormat::R16G16B16A16_Float, Extent});
    Graph.Resources.push_back({Plan.Output.Name, ERenderGraphResourceKind::Texture,
        ERenderGraphResourceOwnership::Exported, Plan.Output.Format, Extent});
    if (Plan.FindPass(EDeferredPassStage::ValidationReadback))
    {
        Graph.Resources.push_back({"ReadbackBuffers", ERenderGraphResourceKind::Buffer,
            ERenderGraphResourceOwnership::Exported, Stoner::RHI::ERHIFormat::Unknown, {}});
    }

    for (const FDeferredPassRecord& PlanPass : Plan.Passes)
    {
        const ERenderGraphPassType Type = PlanPass.Stage == EDeferredPassStage::ValidationReadback
            ? ERenderGraphPassType::Copy : ERenderGraphPassType::Graphics;
        Graph.Passes.push_back({PlanPass.Name, PlanPass.Stage, Type,
            PlanPass.bCullEligible && PlanPass.DrawCount == 0 && PlanPass.LightCount == 0});
        for (const Stoner::Core::FString& Read : PlanPass.Reads)
        {
            if (!Graph.FindResource(Read))
            {
                if (Diagnostics)
                {
                    Diagnostics->Add(EDeferredDiagnosticSeverity::Error, PlanPass.Stage,
                        EDeferredResult::GraphCompilationFailed, "DEF-GRAPH-READ", Read,
                        "pass reads an undeclared resource");
                }
                return Graph;
            }
            Graph.Accesses.push_back({PlanPass.Name, Read, ERenderGraphAccessType::Read,
                ERenderGraphResourceState::Read});
        }
        for (const Stoner::Core::FString& Write : PlanPass.Writes)
        {
            if (!Graph.FindResource(Write))
            {
                if (Diagnostics)
                {
                    Diagnostics->Add(EDeferredDiagnosticSeverity::Error, PlanPass.Stage,
                        EDeferredResult::GraphCompilationFailed, "DEF-GRAPH-WRITE", Write,
                        "pass writes an undeclared resource");
                }
                return Graph;
            }
            Graph.Accesses.push_back({PlanPass.Name, Write, ERenderGraphAccessType::Write,
                ERenderGraphResourceState::Write});
        }
    }
    Graph.SceneColorOutput = Plan.Output.Name;
    Graph.FinalOutput.Clear();
    Graph.bValid = !Graph.SceneColorOutput.IsEmpty() &&
        Graph.FindResource(Graph.SceneColorOutput) != nullptr;
    return Graph;
}

} // namespace Stoner::Renderer
