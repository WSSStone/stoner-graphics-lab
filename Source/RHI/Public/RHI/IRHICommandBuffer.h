#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIQueueType.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{

enum class ERHICommandBufferState
{
    Idle,
    Recording,
    Completed,
    Submitted,
    Resettable
};

enum class ERHISymbolicCommandType
{
    Draw,
    Dispatch,
    Barrier
};

class IRHICommandBuffer
{
public:
    virtual ~IRHICommandBuffer() = default;

    [[nodiscard]] virtual ERHICommandBufferState GetState() const noexcept = 0;
    [[nodiscard]] virtual ERHIQueueType GetCompatibleQueueType() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetRecordedCommandCount() const noexcept = 0;

    virtual ERHIResult Begin() = 0;
    virtual ERHIResult End() = 0;
    virtual ERHIResult Reset() = 0;

    virtual ERHIResult RecordDraw(Stoner::Core::uint32 VertexCount, Stoner::Core::uint32 InstanceCount = 1) = 0;
    virtual ERHIResult RecordDispatch(Stoner::Core::uint32 GroupCountX, Stoner::Core::uint32 GroupCountY, Stoner::Core::uint32 GroupCountZ) = 0;
    virtual ERHIResult RecordBarrier() = 0;
};

} // namespace Stoner::RHI
