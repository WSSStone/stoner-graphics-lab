#include "FAssetWorkerExecutor.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace Stoner::Asset::Private
{

struct FAssetWorkerExecutor::FImpl
{
    explicit FImpl(Core::uint32 Capacity) : QueueCapacity(Capacity) {}

    std::mutex Mutex;
    std::condition_variable Wake;
    std::deque<std::function<void()>> Queue;
    std::vector<std::thread> Workers;
    Core::uint32 QueueCapacity = 0;
    bool StopRequested = false;
    bool Joined = false;
};

FAssetWorkerExecutor::FAssetWorkerExecutor(
    Core::uint32 WorkerCount,
    Core::uint32 QueueCapacity)
    : Impl_(Core::MakeUnique<FImpl>(QueueCapacity))
{
    if (WorkerCount == 0 || QueueCapacity == 0)
    {
        Impl_->StopRequested = true;
        Impl_->Joined = true;
        return;
    }
    Impl_->Workers.reserve(WorkerCount);
    for (Core::uint32 Index = 0; Index < WorkerCount; ++Index)
    {
        Impl_->Workers.emplace_back([State = Impl_.get()] {
            for (;;)
            {
                std::function<void()> Work;
                {
                    std::unique_lock Lock(State->Mutex);
                    State->Wake.wait(Lock, [State] {
                        return State->StopRequested || !State->Queue.empty();
                    });
                    if (State->Queue.empty())
                    {
                        if (State->StopRequested) return;
                        continue;
                    }
                    Work = std::move(State->Queue.front());
                    State->Queue.pop_front();
                }
                Work();
            }
        });
    }
}

FAssetWorkerExecutor::~FAssetWorkerExecutor()
{
    RequestStop();
    Join();
}

EAssetWorkerSubmitResult FAssetWorkerExecutor::Submit(
    std::function<void()> Work)
{
    if (!Work) return EAssetWorkerSubmitResult::QueueFull;
    {
        std::lock_guard Lock(Impl_->Mutex);
        if (Impl_->StopRequested) return EAssetWorkerSubmitResult::Stopped;
        if (Impl_->Queue.size() >= Impl_->QueueCapacity)
            return EAssetWorkerSubmitResult::QueueFull;
        Impl_->Queue.push_back(std::move(Work));
    }
    Impl_->Wake.notify_one();
    return EAssetWorkerSubmitResult::Accepted;
}

void FAssetWorkerExecutor::RequestStop() noexcept
{
    {
        std::lock_guard Lock(Impl_->Mutex);
        Impl_->StopRequested = true;
    }
    Impl_->Wake.notify_all();
}

void FAssetWorkerExecutor::Join() noexcept
{
    for (auto& Worker : Impl_->Workers)
        if (Worker.joinable()) Worker.join();
    std::lock_guard Lock(Impl_->Mutex);
    Impl_->Joined = true;
}

bool FAssetWorkerExecutor::IsJoined() const noexcept
{
    std::lock_guard Lock(Impl_->Mutex);
    return Impl_->Joined;
}

} // namespace Stoner::Asset::Private
