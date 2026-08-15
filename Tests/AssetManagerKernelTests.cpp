#include "AssetManagerKernelTests.h"

#include "Asset/FAssetRequestHandle.h"
#include "FAssetRequestTable.h"
#include "FAssetWorkerExecutor.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;
using namespace std::chrono_literals;

void Record(FAssetManagerKernelTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestBoundedFifoExecutor(FAssetManagerKernelTestResult& Result)
{
    FAssetWorkerExecutor Executor(1, 2);
    std::mutex Mutex;
    std::condition_variable Condition;
    bool FirstStarted = false;
    bool ReleaseFirst = false;
    std::vector<int> Order;

    const auto Blocking = Executor.Submit([&] {
        std::unique_lock Lock(Mutex);
        FirstStarted = true;
        Condition.notify_one();
        Condition.wait(Lock, [&] { return ReleaseFirst; });
        Order.push_back(1);
    });
    {
        std::unique_lock Lock(Mutex);
        Condition.wait_for(Lock, 2s, [&] { return FirstStarted; });
    }
    const auto Second = Executor.Submit([&] { Order.push_back(2); });
    const auto Third = Executor.Submit([&] { Order.push_back(3); });
    const auto Rejected = Executor.Submit([] {});

    {
        std::lock_guard Lock(Mutex);
        ReleaseFirst = true;
    }
    Condition.notify_one();
    Executor.RequestStop();
    Executor.Join();

    Record(Result,
        Blocking == EAssetWorkerSubmitResult::Accepted &&
            Second == EAssetWorkerSubmitResult::Accepted &&
            Third == EAssetWorkerSubmitResult::Accepted &&
            Rejected == EAssetWorkerSubmitResult::QueueFull,
        "worker admission is bounded before queue mutation");
    Record(Result, Order == std::vector<int>({1, 2, 3}),
        "single worker executes accepted work in FIFO order");
    Record(Result,
        Executor.Submit([] {}) == EAssetWorkerSubmitResult::Stopped,
        "stopped executor rejects new work");
    Record(Result, Executor.IsJoined(),
        "join leaves no manager-owned worker alive");
}

void TestRequestSlotGenerationAndTerminalCommit(
    FAssetManagerKernelTestResult& Result)
{
    FAssetRequestTable Table(0x260026ULL, 1);
    FAssetRequestHandle First;
    Record(Result, Table.Allocate(First) && First.IsValid(),
        "request table allocates a generation-safe capability");
    Record(Result,
        Table.Transition(First, EAssetRequestState::Accepted,
            EAssetRequestState::Loading),
        "request state transition validates its expected source state");
    Record(Result,
        Table.CommitTerminal(
            First, EAssetRequestState::Ready, EAssetResult::Success),
        "terminal result commits once");
    Record(Result,
        !Table.CommitTerminal(
            First, EAssetRequestState::Failed, EAssetResult::ProcessingFailure),
        "terminal result cannot be overwritten");

    FAssetRequestSnapshot Snapshot;
    Record(Result,
        Table.Query(First, Snapshot) &&
            Snapshot.State == EAssetRequestState::Ready &&
            Snapshot.Result == EAssetResult::Success,
        "terminal query is stable and idempotent");
    Record(Result, Table.Release(First), "request slot can be released once");

    FAssetRequestHandle Reused;
    Record(Result,
        Table.Allocate(Reused) && Reused.IsValid() && Reused != First,
        "slot reuse increments request generation");
    Record(Result,
        !Table.Query(First, Snapshot) && !Table.Release(First),
        "stale request capability cannot observe or release reused slot");

    FAssetRequestTable ForeignTable(0x260027ULL, 1);
    Record(Result, !ForeignTable.Query(Reused, Snapshot),
        "foreign manager lifetime rejects an otherwise matching slot");
}
} // namespace

FAssetManagerKernelTestResult RunAssetManagerKernelTests()
{
    FAssetManagerKernelTestResult Result;
    TestBoundedFifoExecutor(Result);
    TestRequestSlotGenerationAndTerminalCommit(Result);
    return Result;
}
