#include "Renderer/FRenderGraphBuilder.h"

#include "Renderer/FRenderGraph.h"

namespace Stoner::Renderer
{

FRenderGraphBuilder::FRenderGraphBuilder(FRenderGraph& InGraph)
    : Graph(InGraph)
{
}

FRenderGraphResourceHandle FRenderGraphBuilder::CreateResource(const FRenderGraphResourceDesc& Desc)
{
    return Graph.AddResource(Desc);
}

FRenderGraphResourceHandle FRenderGraphBuilder::ImportResource(const FRenderGraphResourceDesc& Desc)
{
    FRenderGraphResourceDesc ImportDesc = Desc;
    ImportDesc.Ownership = ERenderGraphResourceOwnership::Imported;
    if (ImportDesc.InitialState == ERenderGraphResourceState::Unknown)
    {
        ImportDesc.InitialState = ERenderGraphResourceState::External;
    }
    ImportDesc.AliasPolicy = ERenderGraphAliasPolicy::Disabled;
    return Graph.AddResource(ImportDesc);
}

FRenderGraphResourceHandle FRenderGraphBuilder::CreateTexture(
    Stoner::Core::FString Name,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::RHI::ERHIFormat Format,
    Stoner::RHI::ERHISampleCount SampleCount,
    Stoner::RHI::ERHITextureUsage Usage,
    ERenderGraphColorDomain ColorDomain)
{
    return CreateResource(FRenderGraphResourceDesc::TypedTexture2D(
        std::move(Name), Width, Height, Format, SampleCount, Usage, ColorDomain));
}

FRenderGraphPassHandle FRenderGraphBuilder::AddPass(const FRenderGraphPassDesc& Desc)
{
    return Graph.AddPass(Desc);
}

ERenderGraphResult FRenderGraphBuilder::AddAccess(FRenderGraphPassHandle Pass, FRenderGraphResourceHandle Resource, ERenderGraphAccessType Access)
{
    return Graph.AddAccess(Pass, Resource, Access);
}

ERenderGraphResult FRenderGraphBuilder::AddDependency(FRenderGraphPassHandle Before, FRenderGraphPassHandle After)
{
    return Graph.AddDependency(Before, After);
}

ERenderGraphResult FRenderGraphBuilder::MarkOutput(FRenderGraphResourceHandle Resource)
{
    return Graph.AddOutput(Resource);
}

} // namespace Stoner::Renderer
