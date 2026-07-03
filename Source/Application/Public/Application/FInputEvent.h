#pragma once

#include "Application/EKey.h"
#include "Application/EMouseButton.h"
#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

enum class EInputEventType
{
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    PointerMove,
    Scroll,
    FocusLost,
    Unknown
};

struct FInputEvent
{
    EInputEventType EventType = EInputEventType::Unknown;
    EKey Key = EKey::Unknown;
    EMouseButton MouseButton = EMouseButton::Unknown;
    float PointerX = 0.0f;
    float PointerY = 0.0f;
    float DeltaX = 0.0f;
    float DeltaY = 0.0f;
    Stoner::Core::uint64 Sequence = 0;

    [[nodiscard]] static FInputEvent KeyDown(EKey Key, Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FInputEvent KeyUp(EKey Key, Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FInputEvent MouseDown(EMouseButton Button, Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FInputEvent MouseUp(EMouseButton Button, Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FInputEvent PointerMove(float X, float Y, Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FInputEvent Scroll(float DeltaX, float DeltaY, Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FInputEvent FocusLost(Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FInputEvent Unknown(Stoner::Core::uint64 Sequence = 0);
};

[[nodiscard]] const char* ToString(EInputEventType Type) noexcept;
void SortInputEventsStable(Stoner::Core::TArray<FInputEvent>& Events);

} // namespace Stoner::Application
