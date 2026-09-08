#pragma once
#include "Renderer/FUIDrawSnapshot.h"
#include <functional>

namespace Stoner::Renderer
{
struct FUIDrawValidationContext
{
    Stoner::Core::uint64 SessionId = 0;
    Stoner::Core::uint64 SettingsRevision = 0;
    Stoner::Core::uint64 DisplayGeneration = 0;
    Stoner::Core::uint64 LastSubmittedFrameId = 0;
    Stoner::Core::uint32 DrawableWidth = 0;
    Stoner::Core::uint32 DrawableHeight = 0;
    // The caller owns these generation leases until every upload/render use
    // completes. This check must not infer readiness from a slot number alone.
    std::function<bool(FUITextureId)> HasTextureLease;
};
struct FUIValidatedCommand
{
    FUIDrawCommand Draw;
    Stoner::Core::uint32 ScissorX = 0;
    Stoner::Core::uint32 ScissorY = 0;
    Stoner::Core::uint32 ScissorWidth = 0;
    Stoner::Core::uint32 ScissorHeight = 0;
};
struct FUIDrawValidationResult
{
    bool bValid = false;
    Stoner::Core::FString Diagnostic;
    Stoner::Core::TArray<FUIValidatedCommand> Commands;
};
class FUIDrawValidator
{
public:
    [[nodiscard]] static FUIDrawValidationResult Validate(
        const FUIDrawSnapshot& Snapshot, const FUIDrawValidationContext& Context);
};
}
