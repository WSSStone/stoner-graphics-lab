#pragma once

#include "Renderer/FDeferredFrameExecutor.h"
#include "Renderer/FStaticModelRealization.h"

namespace Stoner::Renderer
{

// Owns the mutable frame/draw uniform resources for one quiescent frame slot.
// The model snapshot and every immutable mesh, texture, shader, pipeline and
// material descriptor remain shared.  Callers must not Update or Release
// while commands using this slot are still pending on the GPU.
class FDeferredFrameUniformResources final
{
public:
    FDeferredFrameUniformResources() = default;
    ~FDeferredFrameUniformResources();

    FDeferredFrameUniformResources(const FDeferredFrameUniformResources&) = delete;
    FDeferredFrameUniformResources& operator=(
        const FDeferredFrameUniformResources&) = delete;
    FDeferredFrameUniformResources(FDeferredFrameUniformResources&&) = delete;
    FDeferredFrameUniformResources& operator=(
        FDeferredFrameUniformResources&&) = delete;

    [[nodiscard]] RHI::ERHIResult Initialize(
        const Core::TSharedPtr<RHI::IRHIDevice>& Device,
        const Core::TSharedPtr<const FStaticModelRenderSnapshot>& Snapshot,
        const FDeferredFramePlan& Plan,
        Core::FString* OutReason = nullptr);

    // Update is valid only after the caller has proved that this slot is
    // quiescent. Invalid input is rejected before upload; a native upload
    // failure invalidates this slot's mutable resources. Neither path mutates
    // or invalidates the shared snapshot.
    [[nodiscard]] RHI::ERHIResult Update(
        const Core::TSharedPtr<RHI::IRHIDevice>& Device,
        const FDeferredFramePlan& Plan,
        Core::FString* OutReason = nullptr);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] const Core::TArray<FDeferredSurfaceDrawBinding>&
        GetSurfaceDraws() const noexcept;
    [[nodiscard]] const Core::TArray<Core::TSharedPtr<RHI::IRHIBuffer>>&
        GetOwnedBuffers() const noexcept;
    [[nodiscard]] const Core::TSharedPtr<RHI::IRHIBuffer>&
        GetFrameUniformBuffer() const noexcept;

    // Release is a caller-coordinated, quiescent operation. It invalidates
    // resources created by this helper, then drops its references; pending
    // native submissions must retain their own owners.
    void Release() noexcept;

private:
    void InvalidateMutableResources() noexcept;

    Core::TSharedPtr<RHI::IRHIDevice> Device_;
    Core::TSharedPtr<const FStaticModelRenderSnapshot> Snapshot_;
    Core::TArray<FDeferredSurfaceDrawBinding> SurfaceDraws_;
    Core::TArray<Core::TSharedPtr<RHI::IRHIBuffer>> OwnedBuffers_;
    Core::TArray<Core::TSharedPtr<RHI::IRHIDescriptorSet>> OwnedDescriptors_;
    Core::TArray<const RHI::IRHIBuffer*> MutableSourceBuffers_;
    Core::TArray<Core::uint32> MutableDrawSlots_;
    Core::TArray<bool> MutableIsFrame_;
    Core::TArray<Core::uint32> DrawSlots_;
    Core::TArray<FDeferredEntityIdentity> DrawIdentities_;
    Core::TSharedPtr<RHI::IRHIBuffer> FrameUniformBuffer_;
    Core::FString FrameId_;
    bool bValid_ = false;
};

} // namespace Stoner::Renderer
