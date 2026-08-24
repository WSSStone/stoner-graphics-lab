#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{
class IRHICommandBuffer;
class IRHICommandQueue;
class IRHIDevice;
class IRHIFence;
}

namespace Stoner::Demo
{

class FProductionSubmissionHarness
{
public:
    [[nodiscard]] RHI::ERHIResult Initialize(
        const Core::TSharedPtr<RHI::IRHIDevice>& Device);
    [[nodiscard]] RHI::ERHIResult SubmitAndWait(
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& Commands,
        Core::uint64 TimeoutMicroseconds);
    void Release() noexcept;

private:
    Core::TSharedPtr<RHI::IRHICommandQueue> Queue;
    Core::TSharedPtr<RHI::IRHIFence> Fence;
};

} // namespace Stoner::Demo
