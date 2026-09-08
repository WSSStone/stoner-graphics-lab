#pragma once
#include "FUIDrawValidator.h"
#include "FUITextureRegistry.h"
#include "Renderer/FUICompositionSettings.h"
#include "RHI/FRHIShaderModuleDesc.h"

namespace Stoner::Renderer
{
// Slot-owned prepared resources. The caller retains them through its exact
// render fence, then releases them independently of presentation retirement.
class FUICompositionFrame
{
public:
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> GetOutput() const noexcept;
    [[nodiscard]] bool HasDraws() const noexcept;
    [[nodiscard]] bool CanRecord(const FUITextureRegistry& Registry) const noexcept;
private:
    struct FImpl;
    Stoner::Core::TSharedPtr<FImpl> Impl;
    friend class FUICompositionExecutor;
};
// A slot-owned diagnostic producer. Retain through the submitting render fence.
class FUIDiagnosticFrame
{
public:
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> GetOutput() const noexcept;
    [[nodiscard]] bool CanRecord() const noexcept;
private:
    struct FImpl;
    Stoner::Core::TSharedPtr<FImpl> Impl;
    friend class FUICompositionExecutor;
};
class FUICompositionExecutor
{
public:
    [[nodiscard]] static Stoner::RHI::ERHIResult PrepareDiagnostic(
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Source,
        const FResolvedOutputTransformDebugBypass& Selection,
        std::span<const Stoner::RHI::FRHIShaderModuleDesc> Shaders,
        Stoner::Core::uint64 RemainingAttachmentBytes, FUIDiagnosticFrame& OutFrame);
    // Source is ShaderReadOnly in this command stream; output leaves it sampled.
    // Failure requires discarding the entire command before releasing the frame.
    [[nodiscard]] static Stoner::RHI::ERHIResult RecordDiagnostic(FUIDiagnosticFrame& Frame,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& Command);
    // Failure leaves OutFrame untouched; the caller may select original Scene
    // before submission. Empty/clipped packets retain Scene without allocating
    // a composition target. Shader descriptions contain offline owned bytes.
    // When supplied, retain GpuContext.Graph until recording finishes.
    [[nodiscard]] static Stoner::RHI::ERHIResult Prepare(
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
        const FUIDrawSnapshot& Snapshot, const FUIDrawValidationContext& Context,
        const FUICompositionSettings& Settings, FUITextureRegistry& Registry,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Scene,
        std::span<const Stoner::RHI::FRHIShaderModuleDesc> DrawShaders,
        std::span<const Stoner::RHI::FRHIShaderModuleDesc> CopyShaders,
        FUICompositionFrame& OutFrame, const FUIGpuTextureContext* GpuContext = nullptr);
    // Scene must already be ShaderReadOnly in this command stream. On failure
    // discard the command before releasing Submission and Frame; never submit
    // partial recording or report native execution as a successful fallback.
    // After successful queue admission, commit Submission with its render fence.
    [[nodiscard]] static Stoner::RHI::ERHIResult Record(
        FUICompositionFrame& Frame, FUITextureRegistry& Registry,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& Command,
        FUITextureSubmission& Submission);
};
}
