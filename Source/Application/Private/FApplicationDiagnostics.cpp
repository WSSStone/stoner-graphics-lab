#include "Application/FApplicationDiagnostics.h"

#include "Application/FApplicationLoop.h"
#include "Application/FInputManager.h"
#include "Application/FWindow.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Stoner::Application
{

void FApplicationDiagnosticLog::Add(EApplicationDiagnosticSeverity Severity,
    EApplicationDiagnosticCategory Category,
    EApplicationResult Result,
    Stoner::Core::FString StableCode,
    Stoner::Core::FString SubjectName,
    Stoner::Core::FString Message)
{
    Records.push_back({Severity, Category, Result, std::move(StableCode), std::move(SubjectName), std::move(Message)});
}

void FApplicationDiagnosticLog::Merge(const FApplicationDiagnosticLog& Other)
{
    Records.insert(Records.end(), Other.Records.begin(), Other.Records.end());
    SortStable();
}

void FApplicationDiagnosticLog::SortStable()
{
    std::stable_sort(Records.begin(), Records.end(), [](const FApplicationDiagnosticRecord& Left, const FApplicationDiagnosticRecord& Right) {
        if (Left.StableCode != Right.StableCode)
        {
            return Left.StableCode < Right.StableCode;
        }
        if (Left.SubjectName != Right.SubjectName)
        {
            return Left.SubjectName < Right.SubjectName;
        }
        return Left.Message < Right.Message;
    });
}

void FApplicationDiagnosticLog::Clear()
{
    Records.clear();
}

bool FApplicationDiagnosticLog::HasErrors() const noexcept
{
    return std::any_of(Records.begin(), Records.end(), [](const FApplicationDiagnosticRecord& Record) {
        return Record.Severity == EApplicationDiagnosticSeverity::Error;
    });
}

bool FApplicationDiagnosticLog::IsEmpty() const noexcept
{
    return Records.empty();
}

int FApplicationDiagnosticLog::CountByCode(const Stoner::Core::FString& StableCode) const noexcept
{
    return static_cast<int>(std::count_if(Records.begin(), Records.end(), [&StableCode](const FApplicationDiagnosticRecord& Record) {
        return Record.StableCode == StableCode;
    }));
}

const Stoner::Core::TArray<FApplicationDiagnosticRecord>& FApplicationDiagnosticLog::GetRecords() const noexcept
{
    return Records;
}

Stoner::Core::TArray<FApplicationDiagnosticRecord>& FApplicationDiagnosticLog::GetMutableRecords() noexcept
{
    return Records;
}

Stoner::Core::FString FApplicationDiagnosticLog::Format() const
{
    std::ostringstream Stream;
    for (const FApplicationDiagnosticRecord& Record : Records)
    {
        Stream << Record.StableCode.CStr() << '[' << ToString(Record.Severity) << '/'
            << ToString(Record.Category) << '/' << ToString(Record.Result) << "] "
            << Record.SubjectName.CStr() << ": " << Record.Message.CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EApplicationResult Result) noexcept
{
    switch (Result)
    {
    case EApplicationResult::Success: return "Success";
    case EApplicationResult::ValidationFailed: return "ValidationFailed";
    case EApplicationResult::RuntimeUnavailable: return "RuntimeUnavailable";
    case EApplicationResult::InvalidLifecycle: return "InvalidLifecycle";
    case EApplicationResult::UnsupportedMode: return "UnsupportedMode";
    case EApplicationResult::InvalidInput: return "InvalidInput";
    }
    return "Unknown";
}

const char* ToString(EApplicationDiagnosticSeverity Severity) noexcept
{
    switch (Severity)
    {
    case EApplicationDiagnosticSeverity::Info: return "Info";
    case EApplicationDiagnosticSeverity::Warning: return "Warning";
    case EApplicationDiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

const char* ToString(EApplicationDiagnosticCategory Category) noexcept
{
    switch (Category)
    {
    case EApplicationDiagnosticCategory::Window: return "Window";
    case EApplicationDiagnosticCategory::Input: return "Input";
    case EApplicationDiagnosticCategory::Driver: return "Driver";
    case EApplicationDiagnosticCategory::Loop: return "Loop";
    case EApplicationDiagnosticCategory::Validation: return "Validation";
    case EApplicationDiagnosticCategory::RuntimeAvailability: return "RuntimeAvailability";
    case EApplicationDiagnosticCategory::Dump: return "Dump";
    }
    return "Unknown";
}

Stoner::Core::FString BuildApplicationDebugDump(const FWindow& Window,
    const FInputManager& InputManager,
    const FApplicationLoopState* LoopState)
{
    const FInputState& Input = InputManager.GetState();
    std::ostringstream Stream;
    Stream << "ApplicationWindowInput\n";
    Stream << "Window state=" << ToString(Window.GetLifecycleState())
        << " size=" << Window.GetClientWidth() << " by " << Window.GetClientHeight()
        << " mode=" << ToString(Window.GetDisplayMode())
        << " focused=" << (Window.IsFocused() ? "true" : "false")
        << " minimized=" << (Window.IsMinimized() ? "true" : "false")
        << " drawable=" << (Window.HasDrawableArea() ? "true" : "false")
        << " presentationPaused=" << (Window.IsPresentationPaused() ? "true" : "false") << '\n';
    Stream << "Input focused=" << (Input.IsFocused() ? "true" : "false")
        << " heldKeys=" << Input.GetHeldKeys().size()
        << " pressedKeys=" << Input.GetPressedKeys().size()
        << " releasedKeys=" << Input.GetReleasedKeys().size()
        << " heldMouse=" << Input.GetHeldMouseButtons().size()
        << " pointer=" << std::fixed << std::setprecision(3) << Input.GetPointerX() << ',' << Input.GetPointerY()
        << " delta=" << Input.GetPointerDeltaX() << ',' << Input.GetPointerDeltaY() << '\n';
    if (LoopState != nullptr)
    {
        Stream << "Loop frame=" << LoopState->FrameIndex
            << " continue=" << (LoopState->bShouldContinue ? "true" : "false")
            << " closeRequested=" << (LoopState->bCloseRequested ? "true" : "false")
            << " presentationPaused=" << (LoopState->bPresentationPaused ? "true" : "false")
            << " updated=" << (LoopState->bUpdatedThisFrame ? "true" : "false") << '\n';
    }
    Stream << "WindowDiagnostics\n" << Window.GetDiagnostics().Format().CStr();
    Stream << "InputDiagnostics\n" << InputManager.GetDiagnostics().Format().CStr();
    if (LoopState != nullptr)
    {
        Stream << "LoopDiagnostics\n" << LoopState->Diagnostics.Format().CStr();
    }
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::Application
