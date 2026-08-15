#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/TUniquePtr.h"

#include <functional>

namespace Stoner::Asset::Private
{

enum class EAssetWorkerSubmitResult : Core::uint8
{
    Accepted,
    QueueFull,
    Stopped
};

class FAssetWorkerExecutor
{
public:
    FAssetWorkerExecutor(Core::uint32 WorkerCount, Core::uint32 QueueCapacity);
    ~FAssetWorkerExecutor();
    FAssetWorkerExecutor(const FAssetWorkerExecutor&) = delete;
    FAssetWorkerExecutor& operator=(const FAssetWorkerExecutor&) = delete;

    [[nodiscard]] EAssetWorkerSubmitResult Submit(std::function<void()> Work);
    void RequestStop() noexcept;
    void Join() noexcept;
    [[nodiscard]] bool IsJoined() const noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
};

} // namespace Stoner::Asset::Private
