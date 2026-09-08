#pragma once
#include "Renderer/FUIDrawSnapshot.h"
#include "Renderer/FUICompositionSettings.h"
#include "RHI/FRHIShaderModuleDesc.h"
#include <memory>

namespace Stoner::RHI
{
class IRHIDevice;
class IRHITexture;
class IRHICommandBuffer;
class IRHIFence;
}
namespace Stoner::Renderer
{
class FUIRenderSession;
// One prepared terminal pass. Keep this owner through render completion;
// release it independently of the presentation engine's output-image lease.
class FUIRenderFrame
{
public:
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> GetInput() const noexcept;
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> GetOutput() const noexcept;
    [[nodiscard]] const FUICompositionSettings* GetSettings() const noexcept;
    [[nodiscard]] bool HasDraws() const noexcept;
    [[nodiscard]] bool CanRecord() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult Record(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& Command);
    [[nodiscard]] Stoner::RHI::ERHIResult Commit(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>& RenderFence) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ReleaseCompleted() noexcept;
    // The caller must first discard/reset any unsubmitted command recording.
    [[nodiscard]] Stoner::RHI::ERHIResult CancelAfterCommandDiscard() noexcept;
private:
    struct FImpl;
    std::shared_ptr<FImpl> Impl;
    friend class FUIRenderSession;
};
class FUIRenderSession
{
public:
    FUIRenderSession(Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice> Device, Stoner::Core::uint64 SessionId);
    ~FUIRenderSession();
    FUIRenderSession(const FUIRenderSession&) = delete;
    FUIRenderSession& operator=(const FUIRenderSession&) = delete;
    void BeginEligibleFrame(Stoner::Core::uint64 FrameId, bool bEligible) noexcept;
    [[nodiscard]] FUITextureResult PrepareTexture(const FUITextureRequest& Request);
    [[nodiscard]] FUITextureLease AcquireTexture(FUITextureId Id) const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult PrepareFrame(const FUIDrawSnapshot& Snapshot,
        const FUICompositionSettings& Settings, Stoner::Core::uint64 CurrentSettingsRevision,
        Stoner::Core::uint64 LastSubmittedFrameId,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Scene,
        std::span<const Stoner::RHI::FRHIShaderModuleDesc> DrawShaders,
        std::span<const Stoner::RHI::FRHIShaderModuleDesc> CopyShaders,
        Stoner::Core::TSharedPtr<FUIRenderFrame>& OutFrame);
private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};
}
