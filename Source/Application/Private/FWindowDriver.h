#pragma once

#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"

namespace Stoner::Application
{

class IWindowDriver
{
public:
    virtual ~IWindowDriver() = default;

    [[nodiscard]] virtual const char* GetDriverName() const noexcept = 0;
    [[nodiscard]] virtual EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() = 0;
    [[nodiscard]] virtual Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() = 0;
};

} // namespace Stoner::Application
