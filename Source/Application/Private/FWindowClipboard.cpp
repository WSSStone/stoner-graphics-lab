#include "Application/FWindow.h"
#include "Core/FUnicode.h"
#include "FWindowDriver.h"

namespace Stoner::Application
{
namespace
{
bool ValidClipboard(const Stoner::Core::FString& Text)
{
    if (Text.Len() > 65536 || Text.View().find('\0') != std::string_view::npos) return false;
    // Reuse Core's UTF-8 validation; never replace the caller's original bytes
    // with the normalized temporary used by this existing Unicode operation.
    Stoner::Core::FString Normalized;
    return Stoner::Core::FUnicode::NormalizeNFC(Text, Normalized) ==
        Stoner::Core::EUnicodeResult::Success;
}
}

EApplicationResult FWindow::ReadClipboardUtf8(Stoner::Core::FString& OutText)
{
    if (!IsActive()) return EApplicationResult::InvalidLifecycle;
    if (!Driver) return EApplicationResult::UnsupportedMode;
    Stoner::Core::FString Candidate;
    const auto Result = Driver->ReadClipboardUtf8(Candidate);
    if (Result != EApplicationResult::Success) return Result;
    if (!ValidClipboard(Candidate)) return EApplicationResult::InvalidInput;
    OutText = std::move(Candidate);
    return EApplicationResult::Success;
}

EApplicationResult FWindow::WriteClipboardUtf8(const Stoner::Core::FString& Text)
{
    if (!IsActive()) return EApplicationResult::InvalidLifecycle;
    if (!ValidClipboard(Text)) return EApplicationResult::InvalidInput;
    return Driver ? Driver->WriteClipboardUtf8(Text) : EApplicationResult::UnsupportedMode;
}
} // namespace Stoner::Application
