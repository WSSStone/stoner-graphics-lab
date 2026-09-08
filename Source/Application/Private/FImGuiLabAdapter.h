#pragma once
#include "Application/FWindow.h"
#include "FLabInputRouter.h"
#include <memory>

namespace Stoner::Application
{
// Private engine-facing shell. Third-party context/draw pointers never escape.
class FImGuiLabAdapter
{
public:
    FImGuiLabAdapter();
    ~FImGuiLabAdapter();
    FImGuiLabAdapter(const FImGuiLabAdapter&) = delete;
    FImGuiLabAdapter& operator=(const FImGuiLabAdapter&) = delete;
    [[nodiscard]] EApplicationResult Initialize(FWindow& Window);
    [[nodiscard]] EApplicationResult Frame(const Stoner::Core::TArray<FInputEvent>& Events,
        const FWindowDisplayState& Display, double DeltaSeconds);
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
