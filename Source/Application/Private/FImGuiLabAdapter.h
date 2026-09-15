#pragma once
#include "Application/FWindow.h"
#include "FLabInputRouter.h"
#include "FImGuiTextureAdapter.h"
#include "FImGuiDrawAdapter.h"
#include <memory>
#include "Application/FLabSettingsSnapshot.h"
#include "Application/FFreeCameraState.h"
#include <span>

namespace Stoner::Application
{
struct FLabSessionStatistics;
struct FLabPresetActions
{
    bool bPending = false, bNativeActive = false, bCanExport = false;
    const Stoner::Core::FString* Failure = nullptr;
    std::function<bool(const Stoner::Core::FString&)> Import;
    std::function<bool()> Cancel;
    std::function<Stoner::Core::FString(const Stoner::Core::FString&,bool)> Export;
};
// Private engine-facing shell. Third-party context/draw pointers never escape.
class FImGuiLabAdapter
{
public:
    FImGuiLabAdapter();
    ~FImGuiLabAdapter();
    FImGuiLabAdapter(const FImGuiLabAdapter&) = delete;
    FImGuiLabAdapter& operator=(const FImGuiLabAdapter&) = delete;
    [[nodiscard]] EApplicationResult Initialize(FWindow& Window, FImGuiTextureAdapter::FPrepare PrepareTexture = {});
    [[nodiscard]] EApplicationResult Frame(const Stoner::Core::TArray<FInputEvent>& Events,
        const FWindowDisplayState& Display, double DeltaSeconds, bool bRenderEligible = true,
        std::span<const FLabControlSection> Sections = {},
        const std::function<bool(const Stoner::Core::FString&,const Stoner::Core::FString&)>& Invoke = {},
        bool bEditsEnabled = true, const FLabRuntimeInfo* Runtime = nullptr,
        const FFreeCameraState* Camera = nullptr,
        const FLabSettingsSnapshot* Requested = nullptr,
        const FLabSettingsSnapshot* Pending = nullptr,
        const FLabSettingsSnapshot* Effective = nullptr,
        const Stoner::Core::FString* SettingsFailure = nullptr,
        const std::function<bool(const FLabSettingsSnapshot&)>& EditSettings = {},
        const FLabSettingsCapabilities* Capabilities = nullptr,
        const std::function<bool(float,float)>& EditNavigation = {},
        const std::function<bool()>& ResetCamera = {},
        const FLabPresetActions* Presets = nullptr,
        const FLabSessionStatistics* Statistics = nullptr);
    // Hidden/minimized intervals discard stale UI input and unpublished draws.
    void Suspend() noexcept;
    using FAcquireTexture = std::function<Stoner::Renderer::FUITextureLease(Stoner::Renderer::FUITextureId)>;
    [[nodiscard]] Stoner::RHI::ERHIResult ExtractSnapshot(const FAcquireTexture& AcquireTexture,
        Stoner::Renderer::FUIDrawSnapshot& OutSnapshot) const;
    [[nodiscard]] const char* GetTextureDiagnostic() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult GetTextureResult() const noexcept;
    [[nodiscard]] FUILabCapture GetCapture() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetVertexCount() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetFallbackScalarCount() const noexcept;
    [[nodiscard]] EApplicationResult GetClipboardResult() const noexcept;
    [[nodiscard]] Stoner::Core::FString GetText() const;
private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};
}
