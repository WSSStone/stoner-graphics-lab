#pragma once
#include "Renderer/FUIDrawSnapshot.h"
#include <functional>

struct ImDrawData;
struct ImDrawList;
namespace Stoner::Application
{
// Borrowed only during extraction; the range identifies one image primitive
// even when ImGui merges it with neighboring draws using the same texture.
struct FImGuiDiagnosticRange
{
    const ImDrawList* List = nullptr;
    Stoner::Core::uint32 FirstIndex = 0, IndexCount = 0;
};
class FImGuiDrawAdapter
{
public:
    using FResolveTexture = std::function<Stoner::Renderer::FUITextureLease(Stoner::Core::uint64)>;
    // OutSnapshot supplies the immutable frame identity. Failure leaves it
    // untouched; successful extraction exposes no upstream buffers or callbacks.
    [[nodiscard]] static Stoner::RHI::ERHIResult Extract(const ImDrawData& Data,
        const FResolveTexture& ResolveTexture,
        Stoner::Core::uint32 DrawableWidth, Stoner::Core::uint32 DrawableHeight,
        Stoner::Renderer::FUIDrawSnapshot& OutSnapshot,
        const FImGuiDiagnosticRange* Diagnostic = nullptr);
};
}
