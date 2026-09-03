#include "Renderer/FHDRPostProcessPipeline.h"

#include "FOutputTransformShaderParameters.h"

#include "Asset/FAssetDigest.h"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <span>

namespace Stoner::Renderer
{

namespace
{

Stoner::Core::FString BuildFingerprintText(
    const FHDRSceneColorHandoff& SceneColor,
    const FResolvedOutputTransformSettings& Settings,
    const FPostProcessCompositeResolution& PreTonemap,
    const FPostProcessCompositeResolution& PostTonemap,
    const Stoner::Core::TArray<FOutputTransformStage>& Stages)
{
    std::ostringstream Stream;
    Stream.imbue(std::locale::classic());
    Stream << "output-plan-v1|scene=" << SceneColor.GetSceneColorId()
        << "|producer=" << ToString(SceneColor.GetProducer())
        << "|view=" << SceneColor.GetViewId()
        << "|frame=" << SceneColor.GetFrameToken()
        << "|extent=" << SceneColor.GetWidth() << 'x' << SceneColor.GetHeight()
        << "|format=" << static_cast<int>(SceneColor.GetFormat())
        << "|samples=" << static_cast<int>(SceneColor.GetSampleCount())
        << "|exposure=" << std::setprecision(9)
        << Settings.ManualExposureStops
        << "|range=" << ToString(Settings.DynamicRange)
        << "|tone=" << Settings.SDRToneMapVersion.CStr()
        << "|viewing=" << Settings.HDRViewingVersion.CStr()
        << "|profile=" << Settings.OutputDeviceProfileId.CStr()
        << "|profileVersion=" << Settings.OutputDeviceProfileVersion.CStr()
        << "|strategy=" << Settings.TransformStrategyVersion.CStr()
        << "|strategyIndex=" << Settings.TransformStrategyIndex
        << "|format=" << static_cast<int>(Settings.OutputFormat)
        << "|colorSpace=" << static_cast<int>(Settings.ColorSpace)
        << "|native=" << static_cast<int>(Settings.NativeEncoding)
        << "|primaries=" << static_cast<int>(Settings.TargetPrimaries)
        << "|whitePoint=" << static_cast<int>(Settings.WhitePoint)
        << "|transfer=" << static_cast<int>(Settings.Transfer)
        << "|displayDomain=" << static_cast<int>(Settings.DisplayLinearDomain)
        << "|encodedDomain=" << static_cast<int>(Settings.EncodedDomain)
        << "|storage=" << static_cast<int>(Settings.StorageClass)
        << "|metadata=" << static_cast<int>(Settings.MetadataPolicy)
        << "|comparison=" << static_cast<int>(Settings.ComparisonDomain)
        << "|referenceWhite=" << Settings.ReferenceWhiteNits
        << "|peak=" << Settings.TargetPeakNits
        << "|registryId=" << Settings.ProfileRegistryId.CStr()
        << "|registryDigest=" << Settings.ProfileRegistryDigest.CStr()
        << "|vectorSetId=" << Settings.ReferenceVectorSetId.CStr()
        << "|vectorSetDigest=" << Settings.ReferenceVectorSetDigest.CStr()
        << "|vectorManifestDigest=" << Settings.VectorManifestDigest.CStr()
        << "|constantsDigest=" << Settings.TransformConstantsDigest.CStr()
        << "|toleranceId=" << Settings.TolerancePolicyId.CStr()
        << "|toleranceDigest=" << Settings.TolerancePolicyDigest.CStr()
        << "|shader=" << Settings.ShaderVariantId.CStr()
        << "|pipelineKey=" << Settings.PipelineKey.CStr()
        << "|viewApplications="
        << Settings.ViewingTransformApplicationCount
        << "|gamutApplications="
        << Settings.GamutConversionApplicationCount
        << "|transferApplications="
        << Settings.OutputTransferApplicationCount
        << "|present=" << (Settings.bRequirePresentation ? 1 : 0)
        << "|readback=" << (Settings.bRequireReadback ? 1 : 0);
    for (const FOutputTransformStage& Stage : Stages)
    {
        Stream << "|stage=" << Stage.StageId << ':' << ToString(Stage.Kind)
            << ':' << Stage.VersionId.CStr() << ':'
            << ToString(Stage.InputDomain) << ':'
            << ToString(Stage.OutputDomain);
    }
    const auto AppendComposite = [&Stream](const char* Name,
        const FPostProcessCompositeResolution& Composite)
    {
        Stream << '|' << Name << "Count=" << Composite.Operations.size();
        for (const FResolvedPostProcessOperation& Resolved :
             Composite.Operations)
        {
            const FPostProcessOperationDesc& Operation = Resolved.Declaration;
            Stream << '|' << Name << '=' << Operation.OperationId.CStr()
                << ':' << Operation.StrategyVersion.CStr()
                << ':' << Operation.OrderKey
                << ':' << ToString(Operation.InsertionPoint)
                << ':' << ToString(Operation.InputDomain)
                << ':' << ToString(Operation.OutputDomain)
                << ":extent=" << (Operation.bPreservesExtent ? 1 : 0)
                << ":samples=" << (Operation.bPreservesSampleCount ? 1 : 0)
                << ":temporal=" << (Operation.bUsesTemporalState ? 1 : 0);
            for (const auto& Dependency : Operation.DependsOn)
                Stream << ":dep=" << Dependency.CStr();
            for (const auto& Read : Operation.Reads)
                Stream << ":read=" << Read.CStr();
            for (const auto& Write : Operation.Writes)
                Stream << ":write=" << Write.CStr();
        }
    };
    AppendComposite("pre", PreTonemap);
    AppendComposite("post", PostTonemap);
    return Stoner::Core::FString(Stream.str());
}

Stoner::Asset::FAssetDigest DigestText(const Stoner::Core::FString& Text)
{
    const std::string_view View = Text.View();
    return Stoner::Asset::FAssetDigest::FromBytes(
        std::span<const Stoner::Core::uint8>(
            reinterpret_cast<const Stoner::Core::uint8*>(View.data()),
            View.size()));
}

Stoner::Core::uint64 MakeStableId(
    const Stoner::Asset::FAssetDigest& Digest,
    Stoner::Core::uint32 Offset) noexcept
{
    Stoner::Core::uint64 Value = 0;
    const auto& Bytes = Digest.GetBytes();
    for (Stoner::Core::uint32 Index = 0; Index < 8; ++Index)
    {
        Value = (Value << 8U) | Bytes[(Offset + Index) % Bytes.size()];
    }
    return Value == 0 ? 1 : Value;
}

void AppendStage(FOutputTransformPlan& Plan,
    EOutputTransformStageKind Kind,
    const char* Name,
    const Stoner::Core::FString& Version,
    ERenderGraphColorDomain Input,
    ERenderGraphColorDomain Output,
    bool bExternal = false)
{
    Plan.Stages.push_back({
        static_cast<Stoner::Core::uint32>(Plan.Stages.size() + 1), Name,
        Kind, Version, Input, Output, bExternal});
}

bool IsTerminalStage(EOutputTransformStageKind Kind) noexcept
{
    return Kind == EOutputTransformStageKind::FormalReadback ||
        Kind == EOutputTransformStageKind::Presentation;
}

void AppendInsertionDiagnostics(FOutputTransformDiagnosticLog& Diagnostics,
    Stoner::Core::TArray<FOutputTransformInsertionDiagnosticRecord>& Records,
    const FPostProcessCompositeResolution& Composite)
{
    for (const FResolvedPostProcessOperation& Operation : Composite.Operations)
    {
        Records.push_back({Operation.Declaration.OperationId,
            Operation.Declaration.StrategyVersion,
            ToString(Operation.Declaration.InsertionPoint),
            Operation.Declaration.OrderKey, Operation.ResolvedIndex,
            ToString(Operation.ColorDomain),
            static_cast<Stoner::Core::uint32>(
                Operation.Declaration.Reads.size()),
            static_cast<Stoner::Core::uint32>(
                Operation.Declaration.Writes.size()),
            Operation.Declaration.bUsesTemporalState});
        std::ostringstream Message;
        Message.imbue(std::locale::classic());
        Message << "resolvedIndex=" << Operation.ResolvedIndex
            << ",orderKey=" << Operation.Declaration.OrderKey
            << ",domain=" << ToString(Operation.ColorDomain)
            << ",reads=" << Operation.Declaration.Reads.size()
            << ",writes=" << Operation.Declaration.Writes.size()
            << ",temporalState=0";
        Diagnostics.Add(EOutputTransformDiagnosticSeverity::Info,
            EOutputTransformResult::Success, "OT-INSERTION-RESOLVED",
            ToString(Operation.Declaration.InsertionPoint),
            Operation.Declaration.OperationId, Message.str());
    }
}

} // namespace

FOutputTransformShaderParameterPayload
FHDRPostProcessPipeline::BuildShaderParameterPayload(
    const FResolvedOutputTransformSettings& Settings,
    EOutputTransformStageKind Stage) const
{
    using namespace Private;
    EOutputTransformShaderStageMode Mode;
    switch (Stage)
    {
    case EOutputTransformStageKind::ManualExposure:
        Mode = EOutputTransformShaderStageMode::ManualExposure;
        break;
    case EOutputTransformStageKind::SDRToneMap:
    case EOutputTransformStageKind::HDRViewingTransform:
        Mode = EOutputTransformShaderStageMode::ToneOrViewing;
        break;
    case EOutputTransformStageKind::OutputDeviceTransform:
        Mode = EOutputTransformShaderStageMode::OutputDevice;
        break;
    default:
        return {};
    }
    const FOutputTransformShaderParameterBinding Binding =
        FOutputTransformShaderParameterBuilder::Build(Settings, Mode);
    if (!Binding.IsValid()) return {};
    FOutputTransformShaderParameterPayload Out;
    Out.Stage = Stage;
    Out.PipelineKey = Binding.PipelineKey;
    const auto* Begin = reinterpret_cast<const Stoner::Core::uint8*>(
        &Binding.Parameters);
    Out.Bytes.assign(Begin, Begin + sizeof(Binding.Parameters));
    return Out;
}

bool FOutputTransformOutputDesc::IsValid() const noexcept
{
    return Width != 0 && Height != 0 &&
        Stoner::RHI::IsValidRHIFormat(Format) &&
        SampleCount == Stoner::RHI::ERHISampleCount::One &&
        ColorDomain != ERenderGraphColorDomain::Unspecified &&
        AlphaMode == EOutputAlphaMode::OpaqueOne;
}

bool FOutputTransformPlan::IsValid() const noexcept
{
    if (State != EOutputTransformPlanState::Validated || PlanId == 0 ||
        ViewId == 0 || FrameToken == 0 || FormalOutputId == 0 ||
        PlanFingerprint.Len() != 64 || !SceneColor.IsReadyForConsumption() ||
        !ResolvedSettings.IsValid() || !OutputDesc.IsValid() ||
        Stages.size() < 4 || !PreTonemapOperations.Succeeded() ||
        !PostTonemapOperations.Succeeded() || !DiagnosticBypass.IsValid())
    {
        return false;
    }
    if (InsertionDiagnostics.size() !=
            PreTonemapOperations.Operations.size() +
                PostTonemapOperations.Operations.size() ||
        !std::all_of(InsertionDiagnostics.begin(),
            InsertionDiagnostics.end(),
            [](const FOutputTransformInsertionDiagnosticRecord& Record)
            {
                return Record.IsValid();
            }) ||
        ((DiagnosticBypass.Mode == EOutputTransformDebugBypassMode::Disabled) ==
            DiagnosticBypassRecord.IsValid()))
        return false;
    std::set<Stoner::Core::FString> StageNames;
    for (Stoner::Core::uint32 Index = 0; Index < Stages.size(); ++Index)
    {
        const FOutputTransformStage& Stage = Stages[Index];
        if (Stage.StageId != Index + 1 || Stage.Name.IsEmpty() ||
            Stage.InputDomain == ERenderGraphColorDomain::Unspecified ||
            Stage.OutputDomain == ERenderGraphColorDomain::Unspecified ||
            !StageNames.insert(Stage.Name).second)
        {
            return false;
        }
        if ((IsTerminalStage(Stage.Kind) && !Stage.bExternalSideEffect) ||
            (!IsTerminalStage(Stage.Kind) && Stage.bExternalSideEffect))
            return false;
    }

    std::size_t Cursor = 0;
    if (Stages[Cursor++].Kind != EOutputTransformStageKind::SceneColorHandoff ||
        Stages[Cursor++].Kind != EOutputTransformStageKind::ManualExposure)
        return false;
    for (const auto& Operation : PreTonemapOperations.Operations)
    {
        if (Cursor >= Stages.size() ||
            Stages[Cursor].Kind != EOutputTransformStageKind::PreTonemap ||
            Stages[Cursor].Name != Operation.Declaration.OperationId ||
            Stages[Cursor].VersionId != Operation.Declaration.StrategyVersion ||
            Operation.Declaration.bUsesTemporalState)
            return false;
        ++Cursor;
    }
    const EOutputTransformStageKind ExpectedTransform =
        ResolvedSettings.DynamicRange == EOutputDynamicRange::SDR
        ? EOutputTransformStageKind::SDRToneMap
        : EOutputTransformStageKind::HDRViewingTransform;
    if (Cursor >= Stages.size() || Stages[Cursor++].Kind != ExpectedTransform)
        return false;
    for (const auto& Operation : PostTonemapOperations.Operations)
    {
        if (Cursor >= Stages.size() ||
            Stages[Cursor].Kind != EOutputTransformStageKind::PostTonemap ||
            Stages[Cursor].Name != Operation.Declaration.OperationId ||
            Stages[Cursor].VersionId != Operation.Declaration.StrategyVersion ||
            Operation.Declaration.bUsesTemporalState)
            return false;
        ++Cursor;
    }
    if (Cursor >= Stages.size() ||
        Stages[Cursor++].Kind != EOutputTransformStageKind::OutputDeviceTransform)
        return false;
    if (ResolvedSettings.bRequireReadback &&
        (Cursor >= Stages.size() ||
         Stages[Cursor++].Kind != EOutputTransformStageKind::FormalReadback))
        return false;
    if (ResolvedSettings.bRequirePresentation &&
        (Cursor >= Stages.size() ||
         Stages[Cursor++].Kind != EOutputTransformStageKind::Presentation))
        return false;
    return Cursor == Stages.size();
}

const char* ToString(EOutputTransformStageKind Kind) noexcept
{
    switch (Kind)
    {
    case EOutputTransformStageKind::SceneColorHandoff: return "SceneColorHandoff";
    case EOutputTransformStageKind::ManualExposure: return "ManualExposure";
    case EOutputTransformStageKind::PreTonemap: return "PreTonemap";
    case EOutputTransformStageKind::SDRToneMap: return "SDRToneMap";
    case EOutputTransformStageKind::HDRViewingTransform: return "HDRViewingTransform";
    case EOutputTransformStageKind::PostTonemap: return "PostTonemap";
    case EOutputTransformStageKind::OutputDeviceTransform: return "OutputDeviceTransform";
    case EOutputTransformStageKind::FormalReadback: return "FormalReadback";
    case EOutputTransformStageKind::Presentation: return "Presentation";
    }
    return "Unknown";
}

const char* ToString(EOutputTransformPlanState State) noexcept
{
    switch (State)
    {
    case EOutputTransformPlanState::Preparing: return "Preparing";
    case EOutputTransformPlanState::Validated: return "Validated";
    case EOutputTransformPlanState::GraphDeclared: return "GraphDeclared";
    case EOutputTransformPlanState::Bound: return "Bound";
    case EOutputTransformPlanState::Executing: return "Executing";
    case EOutputTransformPlanState::Completed: return "Completed";
    case EOutputTransformPlanState::Published: return "Published";
    case EOutputTransformPlanState::Failed: return "Failed";
    case EOutputTransformPlanState::Released: return "Released";
    }
    return "Unknown";
}

FOutputTransformPrepareResult FHDRPostProcessPipeline::Prepare(
    const FHDRSceneColorHandoff& SceneColor,
    const FOutputTransformSettings& Settings) const
{
    FOutputTransformPrepareResult Out;
    if (!SceneColor.IsReadyForConsumption())
    {
        Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error,
            EOutputTransformResult::InvalidHandoff, "OT-HANDOFF-INVALID",
            "SceneColorHandoff", "SceneColor",
            "SceneColor must be produced with canonical finite metadata");
        return Out;
    }
    const FOutputTransformSettingsValidationResult Validation =
        FOutputTransformSettingsValidator().Validate(Settings);
    Out.Diagnostics.Merge(Validation.Diagnostics);
    if (!Validation.Succeeded())
    {
        Out.Result = Validation.Result;
        return Out;
    }

    FOutputTransformPlan Plan;
    Plan.SceneColor = SceneColor;
    Plan.ViewId = SceneColor.GetViewId();
    Plan.FrameToken = SceneColor.GetFrameToken();
    Plan.ResolvedSettings = Validation.Settings;
    Plan.OutputDesc.Width = SceneColor.GetWidth();
    Plan.OutputDesc.Height = SceneColor.GetHeight();
    Plan.OutputDesc.Format = Validation.Settings.OutputFormat;
    Plan.OutputDesc.SampleCount = Stoner::RHI::ERHISampleCount::One;
    Plan.OutputDesc.ColorDomain = Validation.Settings.EncodedDomain;
    Plan.PreTonemapOperations = Settings.PreTonemapOperations.Resolve(
        EPostProcessInsertionPoint::PreTonemap, SceneColor.GetWidth(),
        SceneColor.GetHeight(), SceneColor.GetSampleCount(),
        ERenderGraphColorDomain::SceneLinearRec709D65);
    Plan.PostTonemapOperations = Settings.PostTonemapOperations.Resolve(
        EPostProcessInsertionPoint::PostTonemap, SceneColor.GetWidth(),
        SceneColor.GetHeight(), SceneColor.GetSampleCount(),
        Validation.Settings.DisplayLinearDomain);
    if (!Plan.PreTonemapOperations.Succeeded() ||
        !Plan.PostTonemapOperations.Succeeded())
    {
        const FPostProcessCompositeResolution& Failed =
            !Plan.PreTonemapOperations.Succeeded()
            ? Plan.PreTonemapOperations : Plan.PostTonemapOperations;
        Out.Result = EOutputTransformResult::InvalidSettings;
        Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error,
            Out.Result, "OT-INSERTION-INVALID", "Prepare",
            ToString(Failed.Result), Failed.StableReason);
        return Out;
    }
    if (!Settings.DiagnosticBypass.IsValid())
    {
        Out.Result = EOutputTransformResult::InvalidSettings;
        Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error,
            Out.Result, "OT-DEBUG-BYPASS-INVALID", "Prepare",
            "DiagnosticBypass", "debug bypass request is not explicit and bounded");
        return Out;
    }
    AppendStage(Plan, EOutputTransformStageKind::SceneColorHandoff,
        "SceneColorHandoff", "SceneColor.LinearRec709D65.RGBA16F.v1",
        ERenderGraphColorDomain::SceneLinearRec709D65,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    AppendStage(Plan, EOutputTransformStageKind::ManualExposure,
        "ManualExposure", "ManualExposure.Stops.v1",
        ERenderGraphColorDomain::SceneLinearRec709D65,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    for (const FResolvedPostProcessOperation& Operation :
         Plan.PreTonemapOperations.Operations)
    {
        AppendStage(Plan, EOutputTransformStageKind::PreTonemap,
            Operation.Declaration.OperationId.CStr(),
            Operation.Declaration.StrategyVersion,
            Operation.ColorDomain, Operation.ColorDomain);
    }
    if (Validation.Settings.DynamicRange == EOutputDynamicRange::SDR)
    {
        AppendStage(Plan, EOutputTransformStageKind::SDRToneMap, "SDRToneMap",
            Validation.Settings.SDRToneMapVersion,
            ERenderGraphColorDomain::SceneLinearRec709D65,
            Validation.Settings.DisplayLinearDomain);
    }
    else
    {
        AppendStage(Plan,
            EOutputTransformStageKind::HDRViewingTransform,
            "HDRViewingTransform", Validation.Settings.HDRViewingVersion,
            ERenderGraphColorDomain::SceneLinearRec709D65,
            Validation.Settings.DisplayLinearDomain);
    }
    for (const FResolvedPostProcessOperation& Operation :
         Plan.PostTonemapOperations.Operations)
    {
        AppendStage(Plan, EOutputTransformStageKind::PostTonemap,
            Operation.Declaration.OperationId.CStr(),
            Operation.Declaration.StrategyVersion,
            Operation.ColorDomain, Operation.ColorDomain);
    }
    AppendStage(Plan, EOutputTransformStageKind::OutputDeviceTransform,
        "OutputDeviceTransform", Validation.Settings.OutputDeviceProfileId,
        Validation.Settings.DisplayLinearDomain,
        Validation.Settings.EncodedDomain);
    if (Validation.Settings.bRequireReadback)
    {
        AppendStage(Plan, EOutputTransformStageKind::FormalReadback,
            "FormalReadback", "ExactGpuCopy.v1",
            Validation.Settings.EncodedDomain,
            Validation.Settings.EncodedDomain, true);
    }
    if (Validation.Settings.bRequirePresentation)
    {
        AppendStage(Plan, EOutputTransformStageKind::Presentation,
            "Presentation", "ExactNativePresentation.v1",
            Validation.Settings.EncodedDomain,
            Validation.Settings.EncodedDomain, true);
    }
    std::set<Stoner::Core::FString> UniqueNames;
    for (const FOutputTransformStage& Stage : Plan.Stages)
    {
        if (!UniqueNames.insert(Stage.Name).second)
        {
            Out.Result = EOutputTransformResult::InvalidSettings;
            Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error,
                Out.Result, "OT-STAGE-NAME-DUPLICATE", "Prepare",
                Stage.Name, "canonical and insertion stage names must be globally unique");
            return Out;
        }
    }

    if (Settings.DiagnosticBypass.Mode !=
        EOutputTransformDebugBypassMode::Disabled)
    {
        const auto Stage = std::find_if(Plan.Stages.begin(), Plan.Stages.end(),
            [&Settings](const FOutputTransformStage& Candidate)
            {
                return Candidate.Name == Settings.DiagnosticBypass.StageName &&
                    !IsTerminalStage(Candidate.Kind);
            });
        if (Stage == Plan.Stages.end())
        {
            Out.Result = EOutputTransformResult::InvalidSettings;
            Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error,
                Out.Result, "OT-DEBUG-STAGE-UNKNOWN", "Prepare",
                Settings.DiagnosticBypass.StageName,
                "diagnostic bypass must select one non-terminal named stage");
            return Out;
        }
        Plan.DiagnosticBypass.SourceStageId = Stage->StageId;
        Plan.DiagnosticBypass.SourceStageName = Stage->Name;
        Plan.DiagnosticBypass.Mode = Settings.DiagnosticBypass.Mode;
        Plan.DiagnosticBypass.SourceDomain = Stage->OutputDomain;
        if (Stage->OutputDomain !=
            ERenderGraphColorDomain::SceneLinearRec709D65)
        {
            Plan.DiagnosticBypass.ReferenceWhiteNits =
                Validation.Settings.ReferenceWhiteNits;
            Plan.DiagnosticBypass.TargetPeakNits =
                Validation.Settings.TargetPeakNits;
        }
        Plan.DiagnosticBypass.VisualizationMinimum =
            Settings.DiagnosticBypass.VisualizationMinimum;
        Plan.DiagnosticBypass.VisualizationMaximum =
            Settings.DiagnosticBypass.VisualizationMaximum;
    }

    const Stoner::Core::FString FormalFingerprintText = BuildFingerprintText(
        SceneColor, Validation.Settings, Plan.PreTonemapOperations,
        Plan.PostTonemapOperations, Plan.Stages);
    const Stoner::Asset::FAssetDigest FormalDigest =
        DigestText(FormalFingerprintText);
    std::ostringstream PlanFingerprintStream;
    PlanFingerprintStream.imbue(std::locale::classic());
    PlanFingerprintStream << FormalFingerprintText.CStr()
        << "|debug=" << ToString(Plan.DiagnosticBypass.Mode)
        << ":stage=" << Plan.DiagnosticBypass.SourceStageName.CStr()
        << ":minimum=" << Plan.DiagnosticBypass.VisualizationMinimum
        << ":maximum=" << Plan.DiagnosticBypass.VisualizationMaximum;
    const Stoner::Asset::FAssetDigest PlanDigest = DigestText(
        Stoner::Core::FString(PlanFingerprintStream.str()));
    Plan.PlanFingerprint = PlanDigest.ToLowerHex();
    Plan.PlanId = MakeStableId(PlanDigest, 0);
    Plan.FormalOutputId = MakeStableId(FormalDigest, 8);
    Plan.State = EOutputTransformPlanState::Validated;
    AppendInsertionDiagnostics(Out.Diagnostics, Plan.InsertionDiagnostics,
        Plan.PreTonemapOperations);
    AppendInsertionDiagnostics(Out.Diagnostics, Plan.InsertionDiagnostics,
        Plan.PostTonemapOperations);
    if (Plan.DiagnosticBypass.Mode !=
        EOutputTransformDebugBypassMode::Disabled)
    {
        Plan.DiagnosticBypassRecord.StageName =
            Plan.DiagnosticBypass.SourceStageName;
        Plan.DiagnosticBypassRecord.SourceDomain =
            ToString(Plan.DiagnosticBypass.SourceDomain);
        Plan.DiagnosticBypassRecord.Mode =
            ToString(Plan.DiagnosticBypass.Mode);
        Plan.DiagnosticBypassRecord.ReferenceWhiteNits =
            Plan.DiagnosticBypass.ReferenceWhiteNits;
        Plan.DiagnosticBypassRecord.TargetPeakNits =
            Plan.DiagnosticBypass.TargetPeakNits;
        std::ostringstream Message;
        Message.imbue(std::locale::classic());
        Message << "mode=" << ToString(Plan.DiagnosticBypass.Mode)
            << ",domain=" << ToString(Plan.DiagnosticBypass.SourceDomain)
            << ",nonAuthoritative=1";
        Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Info,
            EOutputTransformResult::Success, "OT-DEBUG-BYPASS-RESOLVED",
            "DiagnosticBypass", Plan.DiagnosticBypass.SourceStageName,
            Message.str());
    }
    Plan.Diagnostics.Merge(Out.Diagnostics);
    if (!Plan.IsValid())
    {
        Out.Result = EOutputTransformResult::InvalidSettings;
        Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error,
            Out.Result, "OT-PLAN-INVALID", "Prepare", "OutputPlan",
            "resolved plan violated a canonical invariant");
        return Out;
    }
    Out.Plan = std::move(Plan);
    Out.Result = EOutputTransformResult::Success;
    return Out;
}

} // namespace Stoner::Renderer
