#pragma once

#include "Renderer/FOutputTransformGraphDeclaration.h"
#include "Renderer/FOutputTransformPlan.h"
#include "Renderer/FRenderGraph.h"

namespace Stoner::Renderer
{

struct FOutputTransformPrepareResult
{
    EOutputTransformResult Result = EOutputTransformResult::InvalidHandoff;
    FOutputTransformPlan Plan;
    FOutputTransformDiagnosticLog Diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == EOutputTransformResult::Success && Plan.IsValid();
    }
};

// Stable backend-neutral bytes consumed by native RHI bindings. The private
// Renderer implementation remains the sole authority for profile indices,
// transform strategy indices, exposure application, and shader ABI layout.
struct FOutputTransformShaderParameterPayload
{
    EOutputTransformStageKind Stage =
        EOutputTransformStageKind::SceneColorHandoff;
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes;
    Stoner::Core::FString PipelineKey;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return (Stage == EOutputTransformStageKind::ManualExposure ||
                Stage == EOutputTransformStageKind::SDRToneMap ||
                Stage == EOutputTransformStageKind::HDRViewingTransform ||
                Stage == EOutputTransformStageKind::OutputDeviceTransform) &&
            Bytes.size() == 32 && !PipelineKey.IsEmpty();
    }
};

class FHDRPostProcessPipeline
{
public:
    [[nodiscard]] FOutputTransformPrepareResult Prepare(
        const FHDRSceneColorHandoff& SceneColor,
        const FOutputTransformSettings& Settings) const;
    [[nodiscard]] FOutputTransformGraphDeclaration DeclareGraph(
        FRenderGraph& Graph, const FOutputTransformPlan& Plan) const;
    [[nodiscard]] bool ValidateOutputGraph(const FRenderGraph& Graph,
        const FOutputTransformPlan& Plan,
        const FOutputTransformGraphDeclaration& Declaration,
        FOutputTransformDiagnosticLog* Diagnostics = nullptr) const;
    [[nodiscard]] FOutputTransformShaderParameterPayload
    BuildShaderParameterPayload(
        const FResolvedOutputTransformSettings& Settings,
        EOutputTransformStageKind Stage) const;
};

} // namespace Stoner::Renderer
