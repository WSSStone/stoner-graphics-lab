#include "Renderer/FHDRPostProcessPipeline.h"
#include "RHI/FRHIFormatInfo.h"

#include <algorithm>
#include <map>
#include <string>

namespace Stoner::Renderer
{

namespace
{

FRenderGraphPassHandle AddStagePass(FRenderGraphBuilder& Builder,
    const Stoner::Core::FString& Name,
    ERenderGraphPassType Type,
    FRenderGraphResourceHandle Input,
    FRenderGraphResourceHandle Output,
    ERenderGraphExternalSideEffect ExternalSideEffect =
        ERenderGraphExternalSideEffect::None)
{
    FRenderGraphPassDesc Pass = FRenderGraphPassDesc::Make(Name, Type);
    Pass.ExternalSideEffect = ExternalSideEffect;
    Pass.Accesses.push_back({Input, ERenderGraphAccessType::Read,
        ERenderGraphResourceState::Read});
    if (Output.IsValid())
    {
        Pass.Accesses.push_back({Output, ERenderGraphAccessType::Write,
            ERenderGraphResourceState::Write});
    }
    return Builder.AddPass(Pass);
}

void AddGraphFailure(FOutputTransformDiagnosticLog* Diagnostics,
    EOutputTransformResult Result, const char* Code, const char* Message)
{
    if (Diagnostics)
    {
        Diagnostics->Add(EOutputTransformDiagnosticSeverity::Error, Result,
            Code, "Graph", "FormalOutput", Message);
    }
}

const FOutputTransformStage* FindStage(const FOutputTransformPlan& Plan,
    const Stoner::Core::FString& Name)
{
    const auto Stage = std::find_if(Plan.Stages.begin(), Plan.Stages.end(),
        [&Name](const FOutputTransformStage& Candidate)
        {
            return Candidate.Name == Name;
        });
    return Stage == Plan.Stages.end() ? nullptr : &*Stage;
}

bool AddStageResource(FOutputTransformGraphDeclaration& Declaration,
    const FOutputTransformPlan& Plan,
    const Stoner::Core::FString& StageName,
    FRenderGraphResourceHandle Resource)
{
    const FOutputTransformStage* Stage = FindStage(Plan, StageName);
    if (!Stage || !Resource.IsValid()) return false;
    Declaration.StageResources.push_back(
        {Stage->StageId, Stage->Name, Resource});
    return true;
}

Stoner::Core::FString MakeResourceName(const char* Prefix,
    const Stoner::Core::FString& Name, const char* Suffix)
{
    return Stoner::Core::FString(std::string(Prefix) + Name.CStr() + Suffix);
}

FRenderGraphResourceHandle CreateIntermediate(FRenderGraphBuilder& Builder,
    const Stoner::Core::FString& Name,
    const FOutputTransformPlan& Plan,
    ERenderGraphColorDomain Domain)
{
    return Builder.CreateTexture(Name, Plan.OutputDesc.Width,
        Plan.OutputDesc.Height,
        Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
        Stoner::RHI::ERHISampleCount::One,
        Stoner::RHI::ERHITextureUsage::Sampled |
            Stoner::RHI::ERHITextureUsage::ColorAttachment,
        Domain);
}

bool AddInsertionComposite(FRenderGraphBuilder& Builder,
    const FOutputTransformPlan& Plan,
    const FPostProcessCompositeResolution& Composite,
    const char* ResourcePrefix,
    FRenderGraphResourceHandle& CurrentColor,
    Stoner::Core::TArray<FRenderGraphResourceHandle>& ColorOutputs,
    FOutputTransformGraphDeclaration& Declaration)
{
    std::map<Stoner::Core::FString, FRenderGraphResourceHandle>
        AuxiliaryResources;
    std::map<Stoner::Core::FString, FRenderGraphPassHandle> OperationPasses;

    for (const FResolvedPostProcessOperation& Resolved : Composite.Operations)
    {
        const FPostProcessOperationDesc& Operation = Resolved.Declaration;
        const FRenderGraphResourceHandle ColorOutput = CreateIntermediate(
            Builder, MakeResourceName(ResourcePrefix, Operation.OperationId,
                ".Color"), Plan, Operation.OutputDomain);
        if (!ColorOutput.IsValid()) return false;
        ColorOutputs.push_back(ColorOutput);
        Declaration.InsertionResources.push_back(ColorOutput);

        FRenderGraphPassDesc Pass = FRenderGraphPassDesc::Make(
            Operation.OperationId, ERenderGraphPassType::Graphics);
        Pass.Accesses.push_back({CurrentColor, ERenderGraphAccessType::Read,
            ERenderGraphResourceState::Read});
        for (const Stoner::Core::FString& Read : Operation.Reads)
        {
            if (Read == "InputColor") continue;
            const auto Resource = AuxiliaryResources.find(Read);
            if (Resource == AuxiliaryResources.end()) return false;
            Pass.Accesses.push_back({Resource->second,
                ERenderGraphAccessType::Read,
                ERenderGraphResourceState::Read});
        }
        Pass.Accesses.push_back({ColorOutput, ERenderGraphAccessType::Write,
            ERenderGraphResourceState::Write});
        for (const Stoner::Core::FString& Write : Operation.Writes)
        {
            if (Write == "OutputColor") continue;
            const FRenderGraphResourceHandle Auxiliary = CreateIntermediate(
                Builder, MakeResourceName(ResourcePrefix, Write, ".Aux"),
                Plan, Operation.OutputDomain);
            if (!Auxiliary.IsValid() ||
                !AuxiliaryResources.emplace(Write, Auxiliary).second)
                return false;
            Declaration.InsertionResources.push_back(Auxiliary);
            Pass.Accesses.push_back({Auxiliary,
                ERenderGraphAccessType::Write,
                ERenderGraphResourceState::Write});
        }

        const FRenderGraphPassHandle PassHandle = Builder.AddPass(Pass);
        if (!PassHandle.IsValid()) return false;
        Declaration.InsertionPasses.push_back(PassHandle);
        Declaration.OrderedPasses.push_back(PassHandle);
        OperationPasses.emplace(Operation.OperationId, PassHandle);
        for (const Stoner::Core::FString& Dependency : Operation.DependsOn)
        {
            const auto DependencyPass = OperationPasses.find(Dependency);
            if (DependencyPass == OperationPasses.end() ||
                Builder.AddDependency(DependencyPass->second, PassHandle) !=
                    ERenderGraphResult::Success)
                return false;
        }
        if (!AddStageResource(Declaration, Plan, Operation.OperationId,
                ColorOutput))
            return false;
        CurrentColor = ColorOutput;
    }
    return true;
}

std::pair<Stoner::Core::uint32,Stoner::Core::uint32> WidgetExtent(const FOutputTransformPlan& Plan)
{
    using namespace Stoner::Core;
    const uint64 Largest=std::max<uint64>(1024,std::max(Plan.OutputDesc.Width,Plan.OutputDesc.Height));
    return {static_cast<uint32>(std::max<uint64>(1,uint64{Plan.OutputDesc.Width}*1024/Largest)),
        static_cast<uint32>(std::max<uint64>(1,uint64{Plan.OutputDesc.Height}*1024/Largest))};
}
bool AddDiagnosticBypass(FRenderGraphBuilder& Builder,
    const FOutputTransformPlan& Plan,
    FOutputTransformGraphDeclaration& Declaration)
{
    const bool Widget=Plan.HasDiagnosticWidget();
    if (!Widget && !Plan.RequiresDiagnosticReadback()) return true;
    const FRenderGraphResourceHandle Source = Declaration.FindStageResource(
        Plan.DiagnosticBypass.SourceStageId);
    if (!Source.IsValid()) return false;

    FRenderGraphResourceHandle ReadbackSource = Source;
    Stoner::RHI::FRHITextureFootprint Footprint;
    if (Plan.DiagnosticBypass.Mode ==
        EOutputTransformDebugBypassMode::BoundedVisualization)
    {
        const auto [Width,Height]=Widget ? WidgetExtent(Plan) : std::pair{Plan.OutputDesc.Width,Plan.OutputDesc.Height};
        auto Usage=Stoner::RHI::ERHITextureUsage::Sampled | Stoner::RHI::ERHITextureUsage::ColorAttachment;
        if (!Widget) Usage |= Stoner::RHI::ERHITextureUsage::CopySource;
        FRenderGraphResourceDesc DebugOutput =
            FRenderGraphResourceDesc::TypedTexture2D(
                "Output.DiagnosticVisualization", Width, Height,
                Widget ? Stoner::RHI::ERHIFormat::R8G8B8A8_sRGB : Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm,
                Stoner::RHI::ERHISampleCount::One,
                Usage,
                ERenderGraphColorDomain::EncodedSrgb);
        DebugOutput.AliasPolicy = ERenderGraphAliasPolicy::Disabled;
        Declaration.DiagnosticOutput = Builder.CreateResource(DebugOutput);
        if (!Declaration.DiagnosticOutput.IsValid()) return false;
        Declaration.DiagnosticVisualizationPass = AddStagePass(Builder,
            "DiagnosticBoundedVisualization", ERenderGraphPassType::Graphics,
            Source, Declaration.DiagnosticOutput);
        if (!Declaration.DiagnosticVisualizationPass.IsValid()) return false;
        Declaration.OrderedPasses.push_back(
            Declaration.DiagnosticVisualizationPass);
        Declaration.DiagnosticFullscreenPassCount = 1;
        if (Widget)
        {
            Declaration.bDiagnosticWidget=true;
            Declaration.bDiagnosticOutputNonAuthoritative=true;
            return true;
        }
        ReadbackSource = Declaration.DiagnosticOutput;
        if (!Stoner::RHI::TryGetRHITextureFootprint(
                Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm,
                Plan.OutputDesc.Width, Plan.OutputDesc.Height, 1, Footprint))
            return false;
    }
    else
    {
        const Stoner::RHI::ERHIFormat SourceFormat =
            Plan.DiagnosticBypass.SourceDomain ==
                    ERenderGraphColorDomain::SceneLinearRec709D65 ||
                Plan.DiagnosticBypass.SourceDomain ==
                    ERenderGraphColorDomain::DisplayLinearRec709D65 ||
                Plan.DiagnosticBypass.SourceDomain ==
                    ERenderGraphColorDomain::DisplayLinearRec2020D65
            ? Stoner::RHI::ERHIFormat::R16G16B16A16_Float
            : Plan.OutputDesc.Format;
        if (!Stoner::RHI::TryGetRHITextureFootprint(SourceFormat,
                Plan.OutputDesc.Width, Plan.OutputDesc.Height, 1, Footprint))
            return false;
    }

    FRenderGraphResourceDesc Readback = FRenderGraphResourceDesc::Buffer(
        "Output.DiagnosticReadbackBuffer", Footprint.TotalBytes);
    Readback.Ownership = ERenderGraphResourceOwnership::Exported;
    Readback.AliasPolicy = ERenderGraphAliasPolicy::Disabled;
    Declaration.DiagnosticReadbackBuffer = Builder.CreateResource(Readback);
    if (!Declaration.DiagnosticReadbackBuffer.IsValid()) return false;
    Declaration.DiagnosticReadbackPass = AddStagePass(Builder,
        "DiagnosticReadback", ERenderGraphPassType::Copy, ReadbackSource,
        Declaration.DiagnosticReadbackBuffer,
        ERenderGraphExternalSideEffect::Readback);
    if (!Declaration.DiagnosticReadbackPass.IsValid()) return false;
    Declaration.OrderedPasses.push_back(Declaration.DiagnosticReadbackPass);
    Declaration.DiagnosticReadbackCopyCount = 1;
    Declaration.bDiagnosticOutputNonAuthoritative = true;
    return true;
}

} // namespace

FRenderGraphResourceHandle FOutputTransformGraphDeclaration::FindStageResource(
    Stoner::Core::uint32 StageId) const noexcept
{
    const auto Found = std::find_if(StageResources.begin(), StageResources.end(),
        [StageId](const FOutputTransformStageResource& Binding)
        {
            return Binding.StageId == StageId;
        });
    return Found == StageResources.end()
        ? FRenderGraphResourceHandle{} : Found->Resource;
}

bool FOutputTransformGraphDeclaration::IsValid() const noexcept
{
    const Stoner::Core::uint32 ExpectedFullscreen = 3U +
        static_cast<Stoner::Core::uint32>(PreTonemapOutputs.size()) +
        static_cast<Stoner::Core::uint32>(PostTonemapOutputs.size()) +
        DiagnosticFullscreenPassCount + (UIPass.IsValid() ? 1U : 0U);
    const bool bDiagnosticConsistent =
        (!bDiagnosticWidget && !bDiagnosticOutputNonAuthoritative &&
            DiagnosticFullscreenPassCount == 0 && !DiagnosticOutput.IsValid() && !DiagnosticVisualizationPass.IsValid() &&
            DiagnosticReadbackCopyCount == 0 &&
            !DiagnosticReadbackBuffer.IsValid() &&
            !DiagnosticReadbackPass.IsValid()) ||
        (bDiagnosticWidget && bDiagnosticOutputNonAuthoritative && UIPass.IsValid() &&
            DiagnosticFullscreenPassCount == 1 && DiagnosticOutput.IsValid() && DiagnosticVisualizationPass.IsValid() &&
            DiagnosticReadbackCopyCount == 0 && !DiagnosticReadbackBuffer.IsValid() && !DiagnosticReadbackPass.IsValid()) ||
        (!bDiagnosticWidget && bDiagnosticOutputNonAuthoritative &&
            DiagnosticReadbackCopyCount == 1 &&
            DiagnosticReadbackBuffer.IsValid() &&
            DiagnosticReadbackPass.IsValid() &&
            (DiagnosticFullscreenPassCount == 0 ||
             (DiagnosticFullscreenPassCount == 1 &&
              DiagnosticOutput.IsValid() &&
              DiagnosticVisualizationPass.IsValid())));
    return bValid && GraphId != 0 && PlanId != 0 && FormalOutputId != 0 &&
        PlanFingerprint.Len() == 64 && SceneColor.IsValid() &&
        ExposedSceneColor.IsValid() && ToneOrViewedColor.IsValid() &&
        FormalOutput.IsValid() && FormalWriterCount == 1 &&
        UIComposite.IsValid() == UIPass.IsValid() &&
        PreTonemapOutputs.size() <= FPostProcessComposite::MaximumOperations &&
        PostTonemapOutputs.size() <= FPostProcessComposite::MaximumOperations &&
        InsertionPasses.size() ==
            PreTonemapOutputs.size() + PostTonemapOutputs.size() &&
        FullscreenPassCount == ExpectedFullscreen &&
        FullImageVisitCount == ExpectedFullscreen &&
        GpuReadbackCopyCount <= 1 && CpuReadbackInitiationCount == 0 &&
        bDiagnosticConsistent && !OrderedPasses.empty() &&
        std::all_of(StageResources.begin(), StageResources.end(),
            [](const FOutputTransformStageResource& Binding)
            {
                return Binding.IsValid();
            });
}

FOutputTransformGraphDeclaration FHDRPostProcessPipeline::DeclareGraph(
    FRenderGraph& Graph, const FOutputTransformPlan& Plan) const
{
    FOutputTransformGraphDeclaration Out;
    Out.GraphId = Graph.GetGraphId();
    Out.PlanId = Plan.PlanId;
    Out.FormalOutputId = Plan.FormalOutputId;
    Out.PlanFingerprint = Plan.PlanFingerprint;
    if (!Plan.IsValid() || Graph.GetState() != ERenderGraphState::Draft)
        return Out;

    const FRenderGraphResourceRecord* SceneColor =
        Graph.FindResource(Plan.SceneColor.GetResource());
    if (!SceneColor || !SceneColor->Desc.Texture.bIsTyped ||
        SceneColor->Desc.Width != Plan.SceneColor.GetWidth() ||
        SceneColor->Desc.Height != Plan.SceneColor.GetHeight() ||
        SceneColor->Desc.Texture.Format !=
            Stoner::RHI::ERHIFormat::R16G16B16A16_Float ||
        SceneColor->Desc.Texture.SampleCount !=
            Stoner::RHI::ERHISampleCount::One ||
        SceneColor->Desc.Texture.ColorDomain !=
            ERenderGraphColorDomain::SceneLinearRec709D65)
        return Out;

    Stoner::RHI::FRHITextureFootprint ReadbackFootprint;
    if (Plan.ResolvedSettings.bRequireReadback &&
        !Stoner::RHI::TryGetRHITextureFootprint(
            Plan.OutputDesc.Format, Plan.OutputDesc.Width,
            Plan.OutputDesc.Height, 1, ReadbackFootprint))
        return Out;

    FRenderGraphBuilder Builder = Graph.CreateBuilder();
    Out.SceneColor = Plan.SceneColor.GetResource();
    Out.ExposedSceneColor = CreateIntermediate(Builder,
        "Output.ExposedSceneColor", Plan,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    if (!Out.ExposedSceneColor.IsValid() ||
        !AddStageResource(Out, Plan, "SceneColorHandoff", Out.SceneColor) ||
        !AddStageResource(Out, Plan, "ManualExposure", Out.ExposedSceneColor))
        return Out;

    Out.OrderedPasses.push_back(AddStagePass(Builder, "ManualExposure",
        ERenderGraphPassType::Graphics, Out.SceneColor,
        Out.ExposedSceneColor));
    FRenderGraphResourceHandle CurrentColor = Out.ExposedSceneColor;
    if (!AddInsertionComposite(Builder, Plan, Plan.PreTonemapOperations,
            "Output.PreTonemap.", CurrentColor, Out.PreTonemapOutputs, Out))
        return Out;

    Out.ToneOrViewedColor = CreateIntermediate(Builder,
        "Output.ToneOrViewedColor", Plan,
        Plan.ResolvedSettings.DisplayLinearDomain);
    if (!Out.ToneOrViewedColor.IsValid()) return Out;
    const char* ToneOrViewPassName =
        Plan.ResolvedSettings.DynamicRange == EOutputDynamicRange::SDR
        ? "SDRToneMap" : "HDRViewingTransform";
    Out.OrderedPasses.push_back(AddStagePass(Builder, ToneOrViewPassName,
        ERenderGraphPassType::Graphics, CurrentColor,
        Out.ToneOrViewedColor));
    if (!AddStageResource(Out, Plan, ToneOrViewPassName,
            Out.ToneOrViewedColor))
        return Out;
    CurrentColor = Out.ToneOrViewedColor;

    if (!AddInsertionComposite(Builder, Plan, Plan.PostTonemapOperations,
            "Output.PostTonemap.", CurrentColor, Out.PostTonemapOutputs, Out))
        return Out;

    if (Plan.HasDiagnosticWidget() && !AddDiagnosticBypass(Builder,Plan,Out)) return Out;
    if (Plan.TerminalUI)
    {
        Out.UIComposite = CreateIntermediate(Builder,"Output.UIComposite",Plan,Plan.ResolvedSettings.DisplayLinearDomain);
        Out.UIPass = AddStagePass(Builder,"TerminalUI",ERenderGraphPassType::Graphics,CurrentColor,Out.UIComposite);
        if (!Out.UIComposite.IsValid() || !Out.UIPass.IsValid() ||
            !AddStageResource(Out,Plan,"TerminalUI",Out.UIComposite)) return Out;
        if (Out.bDiagnosticWidget && Builder.AddAccess(Out.UIPass,Out.DiagnosticOutput,ERenderGraphAccessType::Read) !=
            ERenderGraphResult::Success) return Out;
        Out.OrderedPasses.push_back(Out.UIPass);
        CurrentColor = Out.UIComposite;
    }

    Stoner::RHI::ERHITextureUsage OutputUsage =
        Stoner::RHI::ERHITextureUsage::ColorAttachment |
        Stoner::RHI::ERHITextureUsage::Sampled;
    if (Plan.ResolvedSettings.bRequireReadback)
        OutputUsage |= Stoner::RHI::ERHITextureUsage::CopySource;
    if (Plan.ResolvedSettings.bRequirePresentation)
        OutputUsage |= Stoner::RHI::ERHITextureUsage::Present;
    FRenderGraphResourceDesc FormalDesc =
        FRenderGraphResourceDesc::TypedTexture2D(
            "Output.FinalOutput", Plan.OutputDesc.Width,
            Plan.OutputDesc.Height, Plan.OutputDesc.Format,
            Plan.OutputDesc.SampleCount, OutputUsage,
            Plan.OutputDesc.ColorDomain);
    FormalDesc.Ownership = ERenderGraphResourceOwnership::Exported;
    FormalDesc.AliasPolicy = ERenderGraphAliasPolicy::Disabled;
    Out.FormalOutput = Builder.CreateResource(FormalDesc);
    if (!Out.FormalOutput.IsValid()) return Out;
    Out.OrderedPasses.push_back(AddStagePass(Builder,
        "OutputDeviceTransform", ERenderGraphPassType::Graphics,
        CurrentColor, Out.FormalOutput));
    if (!AddStageResource(Out, Plan, "OutputDeviceTransform",
            Out.FormalOutput))
        return Out;
    Out.FormalWriterCount = 1;

    if (Plan.RequiresDiagnosticReadback() && !AddDiagnosticBypass(Builder, Plan, Out)) return Out;

    if (Plan.ResolvedSettings.bRequireReadback)
    {
        FRenderGraphResourceDesc Readback = FRenderGraphResourceDesc::Buffer(
            "Output.FormalReadbackBuffer", ReadbackFootprint.TotalBytes);
        Readback.Ownership = ERenderGraphResourceOwnership::Exported;
        Readback.AliasPolicy = ERenderGraphAliasPolicy::Disabled;
        Out.ReadbackBuffer = Builder.CreateResource(Readback);
        Out.ReadbackPass = AddStagePass(Builder, "FormalReadback",
            ERenderGraphPassType::Copy, Out.FormalOutput,
            Out.ReadbackBuffer, ERenderGraphExternalSideEffect::Readback);
        Out.OrderedPasses.push_back(Out.ReadbackPass);
        Out.GpuReadbackCopyCount = 1;
    }
    if (Plan.ResolvedSettings.bRequirePresentation)
    {
        Out.PresentationPass = AddStagePass(Builder, "Presentation",
            ERenderGraphPassType::SideEffect, Out.FormalOutput, {},
            ERenderGraphExternalSideEffect::Presentation);
        Out.OrderedPasses.push_back(Out.PresentationPass);
    }

    Out.FullscreenPassCount = 3U +
        static_cast<Stoner::Core::uint32>(Out.InsertionPasses.size()) +
        Out.DiagnosticFullscreenPassCount + (Out.UIPass.IsValid() ? 1U : 0U);
    Out.FullImageVisitCount = Out.FullscreenPassCount;
    bool bDependenciesValid = true;
    for (Stoner::Core::uint32 Index = 1; Index < Out.OrderedPasses.size(); ++Index)
    {
        bDependenciesValid = bDependenciesValid &&
            Builder.AddDependency(Out.OrderedPasses[Index - 1],
                Out.OrderedPasses[Index]) == ERenderGraphResult::Success;
    }
    const bool bMarked = Builder.MarkOutput(Out.FormalOutput) ==
        ERenderGraphResult::Success;
    Out.bValid = bDependenciesValid && bMarked &&
        std::all_of(Out.OrderedPasses.begin(), Out.OrderedPasses.end(),
            [](FRenderGraphPassHandle Handle) { return Handle.IsValid(); });
    Out.bValid = Out.bValid && ValidateOutputGraph(Graph, Plan, Out);
    return Out;
}

bool FHDRPostProcessPipeline::ValidateOutputGraph(const FRenderGraph& Graph,
    const FOutputTransformPlan& Plan,
    const FOutputTransformGraphDeclaration& Declaration,
    FOutputTransformDiagnosticLog* Diagnostics) const
{
    if (!Plan.IsValid() || Declaration.GraphId != Graph.GetGraphId() ||
        Declaration.PlanId != Plan.PlanId ||
        Declaration.FormalOutputId != Plan.FormalOutputId ||
        Declaration.PlanFingerprint != Plan.PlanFingerprint ||
        Declaration.SceneColor != Plan.SceneColor.GetResource())
    {
        AddGraphFailure(Diagnostics, EOutputTransformResult::InvalidGraph,
            "OT-GRAPH-IDENTITY", "graph plan and handoff identities must agree");
        return false;
    }
    const FRenderGraphResourceRecord* Formal =
        Graph.FindResource(Declaration.FormalOutput);
    if (!Formal || Formal->Desc.Width != Plan.OutputDesc.Width ||
        Formal->Desc.Height != Plan.OutputDesc.Height ||
        Formal->Desc.Texture.Format != Plan.OutputDesc.Format ||
        Formal->Desc.Texture.SampleCount != Plan.OutputDesc.SampleCount ||
        Formal->Desc.Texture.ColorDomain != Plan.OutputDesc.ColorDomain ||
        Formal->Desc.Ownership != ERenderGraphResourceOwnership::Exported)
    {
        AddGraphFailure(Diagnostics, EOutputTransformResult::InvalidGraph,
            "OT-GRAPH-OUTPUT-DESC",
            "formal output descriptor must match the plan exactly");
        return false;
    }

    Stoner::Core::uint32 WriterCount = 0;
    for (const FRenderGraphPassRecord& Pass : Graph.GetPasses())
    {
        for (const FRenderGraphResourceAccess& Access : Pass.Desc.Accesses)
        {
            if (Access.Resource == Declaration.FormalOutput &&
                WritesResource(Access.Access))
                ++WriterCount;
        }
    }
    if (WriterCount != 1 || Declaration.FormalWriterCount != 1)
    {
        AddGraphFailure(Diagnostics,
            EOutputTransformResult::DuplicateFormalWriter,
            "OT-GRAPH-FORMAL-WRITER",
            "one plan must have exactly one formal output writer");
        return false;
    }

    const Stoner::Core::uint32 ExpectedInsertionCount =
        static_cast<Stoner::Core::uint32>(
            Plan.PreTonemapOperations.Operations.size() +
            Plan.PostTonemapOperations.Operations.size());
    const Stoner::Core::uint32 ExpectedDiagnosticFullscreen =
        (Plan.HasDiagnosticWidget() || (Plan.RequiresDiagnosticReadback() &&
            Plan.DiagnosticBypass.Mode == EOutputTransformDebugBypassMode::BoundedVisualization)) ? 1U : 0U;
    const Stoner::Core::uint32 ExpectedFullscreen =
        3U + ExpectedInsertionCount + ExpectedDiagnosticFullscreen + (Plan.TerminalUI ? 1U : 0U);
    const Stoner::Core::uint32 OutputReferences =
        static_cast<Stoner::Core::uint32>(std::count(
            Graph.GetOutputs().begin(), Graph.GetOutputs().end(),
            Declaration.FormalOutput));
    const bool bDiagnosticExpected = Plan.HasDiagnosticWidget() || Plan.RequiresDiagnosticReadback();
    if (OutputReferences != 1 ||
        Declaration.UIComposite.IsValid() != Plan.TerminalUI.has_value() ||
        Declaration.UIPass.IsValid() != Plan.TerminalUI.has_value() ||
        Declaration.InsertionPasses.size() != ExpectedInsertionCount ||
        Declaration.PreTonemapOutputs.size() !=
            Plan.PreTonemapOperations.Operations.size() ||
        Declaration.PostTonemapOutputs.size() !=
            Plan.PostTonemapOperations.Operations.size() ||
        Declaration.FullscreenPassCount != ExpectedFullscreen ||
        Declaration.FullImageVisitCount != ExpectedFullscreen ||
        Declaration.GpuReadbackCopyCount > 1 ||
        Declaration.CpuReadbackInitiationCount != 0 ||
        Declaration.DiagnosticFullscreenPassCount !=
            ExpectedDiagnosticFullscreen ||
        Declaration.DiagnosticReadbackCopyCount !=
            (Plan.RequiresDiagnosticReadback() ? 1U : 0U) ||
        Declaration.bDiagnosticWidget != Plan.HasDiagnosticWidget() ||
        Declaration.bDiagnosticOutputNonAuthoritative != bDiagnosticExpected)
    {
        AddGraphFailure(Diagnostics, EOutputTransformResult::InvalidGraph,
            "OT-GRAPH-BOUNDS",
            "formal output insertion and diagnostic work must remain bounded");
        return false;
    }

    if (Plan.HasDiagnosticWidget())
    {
        const auto* Target=Graph.FindResource(Declaration.DiagnosticOutput);
        const auto* Producer=Graph.FindPass(Declaration.DiagnosticVisualizationPass);
        const auto* UI=Graph.FindPass(Declaration.UIPass);
        const auto [Width,Height]=WidgetExtent(Plan);
        if (!Target || !Producer || !UI || Target->Desc.Width!=Width || Target->Desc.Height!=Height ||
            Target->Desc.Texture.Format!=Stoner::RHI::ERHIFormat::R8G8B8A8_sRGB ||
            Target->Desc.Texture.ColorDomain!=ERenderGraphColorDomain::EncodedSrgb ||
            Target->Desc.Texture.SampleCount!=Stoner::RHI::ERHISampleCount::One ||
            Target->Desc.Texture.Usage!=(Stoner::RHI::ERHITextureUsage::Sampled | Stoner::RHI::ERHITextureUsage::ColorAttachment)) return false;
        const auto Source=Declaration.FindStageResource(Plan.DiagnosticBypass.SourceStageId);
        const auto HasAccess=[](const auto* Pass,auto Resource,auto Access) {
            return std::any_of(Pass->Desc.Accesses.begin(),Pass->Desc.Accesses.end(),[&](const auto& A) {
                return A.Resource==Resource && A.Access==Access; });
        };
        if (!HasAccess(Producer,Source,ERenderGraphAccessType::Read) ||
            !HasAccess(Producer,Declaration.DiagnosticOutput,ERenderGraphAccessType::Write) ||
            !HasAccess(UI,Declaration.DiagnosticOutput,ERenderGraphAccessType::Read)) return false;
        if (Graph.GetState()!=ERenderGraphState::Draft)
        {
            if (Graph.GetState()!=ERenderGraphState::Compiled) return false;
            const auto& Compiled=Graph.GetCompiledGraph();
            const auto Before=std::find(Compiled.ScheduledPasses.begin(),Compiled.ScheduledPasses.end(),Producer->Handle.Index);
            const auto After=std::find(Compiled.ScheduledPasses.begin(),Compiled.ScheduledPasses.end(),UI->Handle.Index);
            if (Before==Compiled.ScheduledPasses.end() || After==Compiled.ScheduledPasses.end() || Before>=After ||
                !std::any_of(Compiled.DependencyEdges.begin(),Compiled.DependencyEdges.end(),[&](const auto& Edge) {
                    return Edge.FromPassIndex==Producer->Handle.Index && Edge.ToPassIndex==UI->Handle.Index; })) return false;
        }
    }

    if (Plan.TerminalUI)
    {
        const auto* Target = Graph.FindResource(Declaration.UIComposite);
        const auto ExpectedInput = Declaration.PostTonemapOutputs.empty()
            ? Declaration.ToneOrViewedColor : Declaration.PostTonemapOutputs.back();
        Stoner::Core::uint32 UIWrites = 0, UIReads = 0, Transfers = 0;
        bool CorrectInput = false;
        for (const auto& Pass : Graph.GetPasses())
        {
            for (const auto& Access : Pass.Desc.Accesses)
            {
                if (Access.Resource == Declaration.UIComposite)
                {
                    if (WritesResource(Access.Access))
                    {
                        ++UIWrites;
                        if (Pass.Handle != Declaration.UIPass) return false;
                    }
                    if (ReadsResource(Access.Access))
                    {
                        ++UIReads;
                        if (Pass.Desc.Name == "OutputDeviceTransform") ++Transfers;
                    }
                }
                if (Pass.Handle == Declaration.UIPass && Access.Resource == ExpectedInput && ReadsResource(Access.Access))
                    CorrectInput = true;
            }
        }
        if (!Target || !Target->Desc.Texture.bIsTyped ||
            Target->Desc.Texture.SampleCount != Stoner::RHI::ERHISampleCount::One || Target->Desc.Width != Plan.OutputDesc.Width || Target->Desc.Height != Plan.OutputDesc.Height ||
            Target->Desc.Texture.Format != Stoner::RHI::ERHIFormat::R16G16B16A16_Float ||
            Target->Desc.Texture.ColorDomain != Plan.ResolvedSettings.DisplayLinearDomain ||
            UIWrites != 1 || UIReads == 0 || Transfers != 1 || !CorrectInput) return false;
    }

    for (const FOutputTransformStage& Stage : Plan.Stages)
    {
        if (Stage.Kind == EOutputTransformStageKind::FormalReadback ||
            Stage.Kind == EOutputTransformStageKind::Presentation)
            continue;
        if (!Declaration.FindStageResource(Stage.StageId).IsValid())
        {
            AddGraphFailure(Diagnostics, EOutputTransformResult::InvalidGraph,
                "OT-GRAPH-STAGE-RESOURCE",
                "every color-producing named stage must resolve one graph resource");
            return false;
        }
    }
    if (bDiagnosticExpected &&
        !Declaration.FindStageResource(
            Plan.DiagnosticBypass.SourceStageId).IsValid())
    {
        AddGraphFailure(Diagnostics, EOutputTransformResult::InvalidGraph,
            "OT-GRAPH-DEBUG-SOURCE",
            "diagnostic bypass source must be a declared named-stage resource");
        return false;
    }
    return true;
}

} // namespace Stoner::Renderer
