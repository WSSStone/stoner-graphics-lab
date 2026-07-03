#pragma once

#include "Application/FWindowDesc.h"

namespace Stoner::Application
{

enum class EWindowEventType
{
    Created,
    Resized,
    Minimized,
    Restored,
    FocusGained,
    FocusLost,
    CloseRequested,
    Destroyed,
    UnavailableRuntime
};

struct FWindowEvent
{
    EWindowEventType EventType = EWindowEventType::Created;
    Stoner::Core::uint32 WindowId = 0;
    Stoner::Core::uint32 ClientWidth = 0;
    Stoner::Core::uint32 ClientHeight = 0;
    Stoner::Core::uint64 Sequence = 0;
    EWindowRuntimeAvailability RuntimeAvailability = EWindowRuntimeAvailability::Available;
    Stoner::Core::FString Message;

    [[nodiscard]] static FWindowEvent Created(Stoner::Core::uint32 WindowId,
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height,
        Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent Resized(Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height,
        Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent Minimized(Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent Restored(Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height,
        Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent FocusGained(Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent FocusLost(Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent CloseRequested(Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent Destroyed(Stoner::Core::uint64 Sequence = 0);
    [[nodiscard]] static FWindowEvent Unavailable(EWindowRuntimeAvailability Availability,
        Stoner::Core::FString Message,
        Stoner::Core::uint64 Sequence = 0);
};

[[nodiscard]] const char* ToString(EWindowEventType Type) noexcept;
void SortWindowEventsStable(Stoner::Core::TArray<FWindowEvent>& Events);

} // namespace Stoner::Application
