#pragma once
#include "FLabCaptureQueue.h"
#include <atomic>
#include <functional>
#include <thread>
namespace Stoner::Demo
{
// One CPU-only export at a time; native handles stay with the run owner.
class FLabCaptureExportTask
{
public:
    ~FLabCaptureExportTask();
    bool Start(std::function<Core::FString()> Work);
    bool Poll(Core::FString& Result);
    bool IsActive() const noexcept { return Worker.joinable(); }
private:
    std::thread Worker;
    std::atomic<bool> Done{false};
    Core::FString Result;
};
struct FLabCaptureEncoded
{
    Core::TArray<Core::uint8> Payload, Report;
    bool bPNG = false;
};
[[nodiscard]] bool EncodeLabCapture(const FLabCaptureCompletion& Completion,
    const Core::TArray<Core::uint8>& Bytes, const Core::FString& SoftwareRevision,
    FLabCaptureEncoded& Out);
}
