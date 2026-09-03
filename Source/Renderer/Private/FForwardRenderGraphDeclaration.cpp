#include "Renderer/FForwardRenderGraphDeclaration.h"

#include "Renderer/FForwardFramePlan.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

namespace
{

bool HasResource(const Stoner::Core::TArray<FForwardResourceDeclaration>& Resources,
    const Stoner::Core::FString& Name)
{
    return std::any_of(Resources.begin(), Resources.end(), [&Name](const FForwardResourceDeclaration& Resource) {
        return Resource.Name == Name;
    });
}

bool HasName(const Stoner::Core::TArray<Stoner::Core::FString>& Names,
    const Stoner::Core::FString& Name)
{
    return std::any_of(Names.begin(), Names.end(), [&Name](const Stoner::Core::FString& Existing) {
        return Existing == Name;
    });
}

void AddUniqueResource(FForwardRenderGraphDeclaration& Declaration,
    Stoner::Core::TArray<FForwardResourceDeclaration>& KnownResources,
    FForwardResourceDeclaration Resource)
{
    if (!HasResource(KnownResources, Resource.Name))
    {
        KnownResources.push_back(Resource);
        Declaration.AddResource(std::move(Resource));
    }
}

void AddMaterialResourceAccesses(FForwardRenderGraphDeclaration& Declaration,
    const Stoner::Core::FString& PassName,
    const Stoner::Core::TArray<FMeshDrawCommand>& Draws)
{
    Stoner::Core::TArray<Stoner::Core::FString> KnownAccesses;
    for (const FMeshDrawCommand& Draw : Draws)
    {
        for (const FMaterialResourceRequirement& Requirement : Draw.GetMaterialBinding().ResourceRequirements)
        {
            const Stoner::Core::FString& ResourceName = Requirement.Reference.ReferenceId;
            if (ResourceName.IsEmpty() || HasName(KnownAccesses, ResourceName))
            {
                continue;
            }
            KnownAccesses.push_back(ResourceName);
            Declaration.AddAccess({PassName, ResourceName, EForwardGraphAccess::Read});
        }
    }
}

} // namespace

void FForwardRenderGraphDeclaration::Clear()
{
    Passes.clear();
    Resources.clear();
    Accesses.clear();
    Outputs.clear();
    SceneColorHandoff = {};
}

void FForwardRenderGraphDeclaration::AddPass(FForwardPassDeclaration Pass)
{
    Passes.push_back(std::move(Pass));
}

void FForwardRenderGraphDeclaration::AddResource(FForwardResourceDeclaration Resource)
{
    Resources.push_back(std::move(Resource));
}

void FForwardRenderGraphDeclaration::AddAccess(FForwardAccessDeclaration Access)
{
    Accesses.push_back(std::move(Access));
}

void FForwardRenderGraphDeclaration::AddOutput(FForwardGraphOutputSummary Output)
{
    Outputs.push_back(std::move(Output));
}

void FForwardRenderGraphDeclaration::SetSceneColorHandoff(
    FForwardGraphOutputSummary Handoff)
{
    SceneColorHandoff = std::move(Handoff);
}

const Stoner::Core::TArray<FForwardPassDeclaration>& FForwardRenderGraphDeclaration::GetPasses() const noexcept
{
    return Passes;
}

const Stoner::Core::TArray<FForwardResourceDeclaration>& FForwardRenderGraphDeclaration::GetResources() const noexcept
{
    return Resources;
}

const Stoner::Core::TArray<FForwardAccessDeclaration>& FForwardRenderGraphDeclaration::GetAccesses() const noexcept
{
    return Accesses;
}

const Stoner::Core::TArray<FForwardGraphOutputSummary>& FForwardRenderGraphDeclaration::GetOutputs() const noexcept
{
    return Outputs;
}

const FForwardGraphOutputSummary&
FForwardRenderGraphDeclaration::GetSceneColorHandoff() const noexcept
{
    return SceneColorHandoff;
}

Stoner::Core::FString FForwardRenderGraphDeclaration::Dump() const
{
    std::ostringstream Stream;
    Stream << "GraphDeclaration\n";
    Stream << "  Passes\n";
    for (const FForwardPassDeclaration& Pass : Passes)
    {
        Stream << "    " << Pass.Name.CStr() << ':' << Pass.StageName.CStr()
            << " draws=" << Pass.DrawCount << '\n';
    }
    Stream << "  Resources\n";
    for (const FForwardResourceDeclaration& Resource : Resources)
    {
        Stream << "    " << Resource.Name.CStr() << ':' << Resource.Kind.CStr()
            << ':' << Resource.FormatSummary.CStr() << '\n';
    }
    Stream << "  Accesses\n";
    for (const FForwardAccessDeclaration& Access : Accesses)
    {
        Stream << "    " << Access.PassName.CStr() << " -> " << Access.ResourceName.CStr()
            << ':' << ToString(Access.Access) << '\n';
    }
    Stream << "  Outputs\n";
    for (const FForwardGraphOutputSummary& Output : Outputs)
    {
        Stream << "    color=" << Output.ColorTargetName.CStr()
            << " depth=" << Output.DepthTargetName.CStr() << '\n';
    }
    Stream << "  SceneColorHandoff\n";
    Stream << "    color=" << SceneColorHandoff.ColorTargetName.CStr()
        << " depth=" << SceneColorHandoff.DepthTargetName.CStr() << '\n';
    return Stoner::Core::FString(Stream.str());
}

FForwardRenderGraphDeclaration BuildForwardRenderGraphDeclaration(const FForwardFramePlan& Plan,
    FForwardDiagnosticLog* Diagnostics)
{
    FForwardRenderGraphDeclaration Declaration;
    Stoner::Core::TArray<FForwardResourceDeclaration> KnownResources;

    AddUniqueResource(Declaration, KnownResources,
        {Plan.OutputTarget.ColorTargetName, "Texture2D", Plan.OutputTarget.FormatSummary});
    if (!Plan.OutputTarget.DepthTargetName.IsEmpty())
    {
        AddUniqueResource(Declaration, KnownResources,
            {Plan.OutputTarget.DepthTargetName, "Texture2D", "Depth"});
    }
    if (Plan.LightSet.HasAcceptedLights() || Plan.AmbientFallback.bActive)
    {
        AddUniqueResource(Declaration, KnownResources,
            {"ForwardLightData", "StructuredData", "DirectionalAndPointLights"});
    }
    if (Plan.Environment.Mode == EForwardBackgroundMode::EnvironmentReference && !Plan.Environment.ResourceReference.IsEmpty())
    {
        AddUniqueResource(Declaration, KnownResources,
            {Plan.Environment.ResourceReference, "Environment", "AbstractReference"});
    }

    auto AddMaterialResources = [&](const Stoner::Core::TArray<FMeshDrawCommand>& Draws) {
        for (const FMeshDrawCommand& Draw : Draws)
        {
            for (const FMaterialResourceRequirement& Requirement : Draw.GetMaterialBinding().ResourceRequirements)
            {
                AddUniqueResource(Declaration, KnownResources,
                    {Requirement.Reference.ReferenceId, ToString(Requirement.Reference.Kind), ToString(Requirement.Reference.Access)});
            }
        }
    };
    AddMaterialResources(Plan.AcceptedOpaqueDraws);
    AddMaterialResources(Plan.AcceptedTransparentDraws);

    for (const FForwardPassRecord& Pass : Plan.PassOrder)
    {
        Declaration.AddPass({Pass.Name, ToString(Pass.Stage), Pass.DrawCount});
        if (Pass.Stage == EForwardPassStage::Depth && !Plan.OutputTarget.DepthTargetName.IsEmpty())
        {
            Declaration.AddAccess({Pass.Name, Plan.OutputTarget.DepthTargetName, EForwardGraphAccess::Write});
        }
        else if (Pass.Stage == EForwardPassStage::Opaque)
        {
            Declaration.AddAccess({Pass.Name, Plan.OutputTarget.ColorTargetName, EForwardGraphAccess::Write});
            if (!Plan.OutputTarget.DepthTargetName.IsEmpty())
            {
                Declaration.AddAccess({Pass.Name, Plan.OutputTarget.DepthTargetName, EForwardGraphAccess::ReadWrite});
            }
            if (Plan.LightSet.HasAcceptedLights() || Plan.AmbientFallback.bActive)
            {
                Declaration.AddAccess({Pass.Name, "ForwardLightData", EForwardGraphAccess::Read});
            }
            AddMaterialResourceAccesses(Declaration, Pass.Name, Plan.AcceptedOpaqueDraws);
        }
        else if (Pass.Stage == EForwardPassStage::SkyBackground)
        {
            Declaration.AddAccess({Pass.Name, Plan.OutputTarget.ColorTargetName, EForwardGraphAccess::Write});
            if (Plan.Environment.Mode == EForwardBackgroundMode::EnvironmentReference && !Plan.Environment.ResourceReference.IsEmpty())
            {
                Declaration.AddAccess({Pass.Name, Plan.Environment.ResourceReference, EForwardGraphAccess::Read});
            }
        }
        else if (Pass.Stage == EForwardPassStage::Transparent)
        {
            Declaration.AddAccess({Pass.Name, Plan.OutputTarget.ColorTargetName, EForwardGraphAccess::ReadWrite});
            if (!Plan.OutputTarget.DepthTargetName.IsEmpty())
            {
                Declaration.AddAccess({Pass.Name, Plan.OutputTarget.DepthTargetName, EForwardGraphAccess::Read});
            }
            if (Plan.LightSet.HasAcceptedLights() || Plan.AmbientFallback.bActive)
            {
                Declaration.AddAccess({Pass.Name, "ForwardLightData", EForwardGraphAccess::Read});
            }
            AddMaterialResourceAccesses(Declaration, Pass.Name, Plan.AcceptedTransparentDraws);
        }
    }

    Declaration.SetSceneColorHandoff(
        {Plan.OutputTarget.ColorTargetName, Plan.OutputTarget.DepthTargetName});
    if (Diagnostics != nullptr)
    {
        Diagnostics->Add(EForwardDiagnosticSeverity::Info, EForwardDiagnosticCategory::ResourceDeclaration,
            EForwardResult::Success, "FWD-GRAPH-DECLARED", Plan.FrameName,
            "render graph-compatible Forward SceneColor producer declarations prepared");
    }
    return Declaration;
}

const char* ToString(EForwardGraphAccess Access) noexcept
{
    switch (Access)
    {
    case EForwardGraphAccess::Read: return "Read";
    case EForwardGraphAccess::Write: return "Write";
    case EForwardGraphAccess::ReadWrite: return "ReadWrite";
    case EForwardGraphAccess::Output: return "Output";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
