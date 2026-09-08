#pragma once
#include "Renderer/FUITextureRequest.h"
#include <array>
#include <functional>
#include <span>

struct ImTextureData;

namespace Stoner::Application
{
// All upstream pointers stay inside Application. Renderer receives only copied
// requests; draw extraction resolves the opaque numeric token before queuing.
class FImGuiTextureAdapter
{
public:
    using FPrepare = std::function<Stoner::Renderer::FUITextureResult(
        const Stoner::Renderer::FUITextureRequest&)>;
    explicit FImGuiTextureAdapter(FPrepare InPrepare);
    ~FImGuiTextureAdapter();
    FImGuiTextureAdapter(const FImGuiTextureAdapter&) = delete;
    FImGuiTextureAdapter& operator=(const FImGuiTextureAdapter&) = delete;
    [[nodiscard]] Stoner::RHI::ERHIResult Cancel() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult Process(
        std::span<ImTextureData* const> Textures,
        Stoner::Core::uint64 FrameId, bool bEligible,
        Stoner::Core::uint64 NowMilliseconds);
    [[nodiscard]] Stoner::Renderer::FUITextureId Resolve(Stoner::Core::uint64 Token) const noexcept;
    [[nodiscard]] const char* GetDiagnostic() const noexcept { return Diagnostic; }
    [[nodiscard]] bool IsFailed() const noexcept { return bFailed; }
private:
    struct FBinding
    {
        ImTextureData* Source = nullptr;
        Stoner::Renderer::FUITextureId Id;
        Stoner::Core::uint64 Token = 0, PendingRequest = 0;
    };
    [[nodiscard]] Stoner::RHI::ERHIResult Fail(const char* Reason = "ui-texture-preparation-unavailable") noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult Defer(Stoner::Core::uint64 Now) noexcept;
    FPrepare Prepare;
    std::array<FBinding, 256> Bindings{};
    Stoner::Core::uint64 LastToken = 0, LastRequest = 0, LastFrame = 0, LastTime = 0, PendingSince = 0;
    Stoner::Core::uint32 RetryFrames = 0;
    bool bPending = false, bFailed = false;
    const char* Diagnostic = "ui-textures-uninitialized";
};
}
