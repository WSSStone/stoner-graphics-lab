#pragma once

#include "FLabProductionFrameContext.h"

#include <functional>

namespace Stoner::Demo
{

// The session callback retains the backend and acknowledges exact logical
// cancellation. It never substitutes an idle assumption for render completion.
using FLabPreviewCancelCallback = std::function<RHI::ERHIResult(
    Core::uint64, Core::uint32, const Core::TSharedPtr<RHI::IRHIFence>&, bool&)>;

// The acquired slot remains session-owned even when this operation fails
// before it can publish a ticket. A valid ticket owns its frame context and
// cancellation callback until render retirement or acknowledged cancellation.
[[nodiscard]] Renderer::FOutputTransformPreviewResult RecordLabProductionPreview(
    const Core::TSharedPtr<FLabProductionFrameContext>& Context,
    const FProductionContentComposition& Composition,
    Core::uint32 Slot,
    const RHI::FRHIResolvedPresentationState& Resolved,
    FLabPreviewCancelCallback Cancel,
    Renderer::FOutputTransformPreviewTicket& OutTicket,
    const FLabProductionFrameContext::FPrepareUI& PrepareUI = {});

} // namespace Stoner::Demo
