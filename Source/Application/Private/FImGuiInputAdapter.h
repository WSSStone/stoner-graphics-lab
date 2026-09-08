#pragma once
#include "Application/FInputEvent.h"
#include "Application/FInputOwnershipSnapshot.h"
#include "imgui.h"

namespace Stoner::Application
{
class FImGuiInputAdapter
{
public:
    void Feed(ImGuiIO& IO, const Stoner::Core::TArray<FInputEvent>& Events, bool bFocused);
private:
    std::array<bool, GInputKeyCount> Held{};
};
}
