#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"
#include "FWindowDriver.h"

namespace Stoner::Application
{

class FGlfwWindowDriver final : public IWindowDriver
{
public:
    [[nodiscard]] const char* GetDriverName() const noexcept override { return "GLFW"; }
    [[nodiscard]] EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept override
    {
        return EWindowRuntimeAvailability::DependencyUnavailable;
    }

    [[nodiscard]] Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() override
    {
        return {FWindowEvent::Unavailable(EWindowRuntimeAvailability::DependencyUnavailable,
            "GLFW dependency is not provisioned for this build",
            1)};
    }

    [[nodiscard]] Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() override
    {
        return {};
    }
};

} // namespace Stoner::Application
