#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Demo
{

struct FDemoProductionCapture;

[[nodiscard]] bool WriteProductionWindowCapture(
    const FDemoProductionCapture& Capture,
    const char* Backend,
    const char* WorkloadRevision,
    const char* Root);

[[nodiscard]] bool ValidateProductionWindowCaptureSet(
    const char* Backend,
    const char* Root,
    Stoner::Core::uint32 ExpectedCount);

} // namespace Stoner::Demo
