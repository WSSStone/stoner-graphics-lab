#pragma once
#include "FLabCaptureQueue.h"
namespace Stoner::Demo
{
struct FLabCaptureEncoded
{
    Core::TArray<Core::uint8> Payload, Report;
    bool bPNG = false;
};
[[nodiscard]] bool EncodeLabCapture(const FLabCaptureCompletion& Completion,
    const Core::TArray<Core::uint8>& Bytes, const Core::FString& SoftwareRevision,
    FLabCaptureEncoded& Out);
}
