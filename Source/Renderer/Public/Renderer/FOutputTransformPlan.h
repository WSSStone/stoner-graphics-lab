#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FHDRSceneColorHandoff.h"
#include "Renderer/FOutputTransformDiagnostics.h"
#include "Renderer/FOutputTransformSettings.h"

namespace Stoner::Renderer
{

enum class EOutputTransformStageKind
{
    SceneColorHandoff,
    ManualExposure,
    PreTonemap,
    SDRToneMap,
    HDRViewingTransform,
    PostTonemap,
    OutputDeviceTransform,
    FormalReadback,
    Presentation
};

enum class EOutputTransformPlanState
{
    Preparing,
    Validated,
    GraphDeclared,
    Bound,
    Executing,
    Completed,
    Published,
    Failed,
    Released
};

struct FOutputTransformStage
{
    Stoner::Core::uint32 StageId = 0;
    Stoner::Core::FString Name;
    EOutputTransformStageKind Kind =
        EOutputTransformStageKind::SceneColorHandoff;
    Stoner::Core::FString VersionId;
    ERenderGraphColorDomain InputDomain =
        ERenderGraphColorDomain::Unspecified;
    ERenderGraphColorDomain OutputDomain =
        ERenderGraphColorDomain::Unspecified;
    bool bExternalSideEffect = false;
};

struct FOutputTransformOutputDesc
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    Stoner::RHI::ERHISampleCount SampleCount =
        Stoner::RHI::ERHISampleCount::One;
    ERenderGraphColorDomain ColorDomain =
        ERenderGraphColorDomain::Unspecified;
    EOutputAlphaMode AlphaMode = EOutputAlphaMode::OpaqueOne;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FOutputTransformPlan
{
    Stoner::Core::uint64 PlanId = 0;
    Stoner::Core::uint64 ViewId = 0;
    Stoner::Core::uint64 FrameToken = 0;
    FHDRSceneColorHandoff SceneColor;
    FResolvedOutputTransformSettings ResolvedSettings;
    FPostProcessCompositeResolution PreTonemapOperations;
    FPostProcessCompositeResolution PostTonemapOperations;
    FResolvedOutputTransformDebugBypass DiagnosticBypass;
    Stoner::Core::TArray<FOutputTransformInsertionDiagnosticRecord>
        InsertionDiagnostics;
    FOutputTransformDiagnosticBypassRecord DiagnosticBypassRecord;
    Stoner::Core::TArray<FOutputTransformStage> Stages;
    Stoner::Core::uint64 FormalOutputId = 0;
    FOutputTransformOutputDesc OutputDesc;
    Stoner::Core::FString PlanFingerprint;
    FOutputTransformDiagnosticLog Diagnostics;
    EOutputTransformPlanState State = EOutputTransformPlanState::Failed;

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] const char* ToString(EOutputTransformStageKind Kind) noexcept;
[[nodiscard]] const char* ToString(EOutputTransformPlanState State) noexcept;

} // namespace Stoner::Renderer
