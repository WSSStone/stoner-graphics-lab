#include "FImGuiLabAdapter.h"
#include "FImGuiInputAdapter.h"
#include "FEmbeddedLabFont.h"
#include "Core/FPlatformProcess.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace Stoner::Application
{
struct FImGuiLabAdapter::FImpl
{
    ImGuiContext* Context = nullptr;
    FWindow* Window = nullptr;
    FImGuiInputAdapter Input;
    FUILabCapture Capture;
    Stoner::Core::FString Clipboard;
    EApplicationResult ClipboardResult = EApplicationResult::Success;
    std::array<char, 65537> Text{};
    Stoner::Core::uint32 VertexCount = 0;
    Stoner::Core::uint64 FallbackCount = 0;
    Stoner::Core::uint64 DisplayGeneration = 0;
    bool bReady = false;
    ~FImpl() { if (Context) ImGui::DestroyContext(Context); }
};

FImGuiLabAdapter::FImGuiLabAdapter() : Impl(std::make_unique<FImpl>()) {}
FImGuiLabAdapter::~FImGuiLabAdapter() = default;

EApplicationResult FImGuiLabAdapter::Initialize(FWindow& Window)
{
    if (!Window.IsActive() || Impl->Context) return EApplicationResult::InvalidLifecycle;
    if (ImGui::GetCurrentContext()) return EApplicationResult::UnsupportedMode;
    ImGui::SetAllocatorFunctions([](size_t Bytes, void*) -> void* {
        if (void* Memory = std::malloc(std::max(Bytes, size_t{1}))) return Memory;
        std::fputs("lab-ui-allocation-failed: terminating before publishing UI\n", stderr);
        Stoner::Core::FPlatformProcess::TerminateCurrentProcess(125);
    }, [](void* Memory, void*) { std::free(Memory); });
    IMGUI_CHECKVERSION();
    Impl->Context = ImGui::CreateContext();
    Impl->Window = &Window;
    auto& IO = ImGui::GetIO();
    IO.IniFilename = nullptr;
    IO.LogFilename = nullptr;
    IO.ConfigInputTrickleEventQueue = false;
    IO.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard;
    // Do not advertise texture support until create/update/destroy acknowledgements
    // are connected. This CPU-only context milestone cannot submit UI geometry.
    IO.BackendFlags = ImGuiBackendFlags_None;
    IO.BackendPlatformName = "Stoner.Application";
    ImFontConfig FontConfig;
    FontConfig.FontDataOwnedByAtlas = false;
    const auto Font = GetEmbeddedLabFont();
    if (Font.empty() || Font.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return EApplicationResult::ValidationFailed;
    IO.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(Font.data()),
        static_cast<int>(Font.size()), 16.0f, &FontConfig);
    unsigned char* Pixels = nullptr;
    int Width = 0, Height = 0;
    IO.Fonts->GetTexDataAsRGBA32(&Pixels, &Width, &Height);
    if (!Pixels || Width <= 0 || Height <= 0 || Width > 2048 || Height > 2048)
        return EApplicationResult::ValidationFailed;
    auto& Platform = ImGui::GetPlatformIO();
    Platform.Platform_ClipboardUserData = Impl.get();
    Platform.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
        auto& State = *static_cast<FImpl*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
        State.ClipboardResult = State.Window->ReadClipboardUtf8(State.Clipboard);
        return State.ClipboardResult == EApplicationResult::Success ? State.Clipboard.CStr() : nullptr;
    };
    Platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* Text) {
        auto& State = *static_cast<FImpl*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
        if (!Text) { State.ClipboardResult = EApplicationResult::InvalidInput; return; }
        std::size_t Bytes = 0;
        while (Bytes <= 65536 && Text[Bytes] != '\0') ++Bytes;
        State.ClipboardResult = Bytes <= 65536 ? State.Window->WriteClipboardUtf8(
            Stoner::Core::FString(std::string_view(Text, Bytes))) : EApplicationResult::InvalidInput;
    };
    Impl->bReady = true;
    return EApplicationResult::Success;
}

EApplicationResult FImGuiLabAdapter::Frame(const Stoner::Core::TArray<FInputEvent>& Events,
    const FWindowDisplayState& Display, double DeltaSeconds)
{
    Impl->Capture = {};
    Impl->VertexCount = 0;
    if (!Impl->bReady || !Impl->Context || !Impl->Window || !Impl->Window->IsActive()) return EApplicationResult::InvalidLifecycle;
    if (Events.size() > 4096 || Display.DisplayGeneration < Impl->DisplayGeneration ||
        !Display.IsValid() || !Display.DrawableExtent.IsPositive() ||
        !Display.LogicalExtent.IsPositive() || !std::isfinite(DeltaSeconds) || DeltaSeconds < 0)
        return EApplicationResult::InvalidInput;
    ImGui::SetCurrentContext(Impl->Context);
    auto& IO = ImGui::GetIO();
    IO.DisplaySize = ImVec2(static_cast<float>(Display.LogicalExtent.Width), static_cast<float>(Display.LogicalExtent.Height));
    IO.DisplayFramebufferScale = ImVec2(
        static_cast<float>(Display.DrawableExtent.Width) / Display.LogicalExtent.Width,
        static_cast<float>(Display.DrawableExtent.Height) / Display.LogicalExtent.Height);
    IO.DeltaTime = static_cast<float>(DeltaSeconds > 0 ? std::clamp(DeltaSeconds, 0.000001, 0.25) : 1.0 / 60.0);
    Impl->Input.Feed(IO, Events, Display.bFocused);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(360, 220), ImGuiCond_Always);
    ImGui::Begin("Rendering Lab", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted("WASD/QE move | RMB look | F1 toggle UI");
    ImGui::InputText("Input test", Impl->Text.data(), Impl->Text.size());
    const bool TextActive = ImGui::IsItemActive();
    // The pinned legacy atlas covers U+0020..U+00FF. Count UTF-8 leading
    // bytes for scalars above that range in the actual retained editable value,
    // including pasted text; do not count rejected/overflowed event payloads.
    Impl->FallbackCount = 0;
    for (unsigned char Byte : std::string_view(Impl->Text.data()))
        if (Byte >= 0xC4 && Byte <= 0xF4) ++Impl->FallbackCount;
    if (Impl->FallbackCount) ImGui::TextUnformatted("Bundled font: some characters use replacement glyphs.");
    if (Impl->ClipboardResult != EApplicationResult::Success)
        ImGui::TextUnformatted("Clipboard unavailable or rejected; text preserved.");
    const bool Active = ImGui::IsAnyItemActive();
    Impl->Capture = {};
    Impl->Capture.bKeyboard = TextActive || (Active && IO.WantCaptureKeyboard);
    Impl->Capture.bTextEditing = TextActive;
    bool MouseDown = false;
    for (bool Down : IO.MouseDown) MouseDown |= Down;
    Impl->Capture.bPointer = IO.WantCaptureMouse && (MouseDown || Active);
    Impl->Capture.bScroll = IO.WantCaptureMouse && (IO.MouseWheel != 0 || IO.MouseWheelH != 0);
    ImGui::End();
    ImGui::Render();
    Impl->DisplayGeneration = Display.DisplayGeneration;
    Impl->VertexCount = static_cast<Stoner::Core::uint32>(ImGui::GetDrawData()->TotalVtxCount);
    return EApplicationResult::Success;
}

FUILabCapture FImGuiLabAdapter::GetCapture() const noexcept { return Impl->Capture; }
Stoner::Core::uint32 FImGuiLabAdapter::GetVertexCount() const noexcept { return Impl->VertexCount; }
Stoner::Core::uint64 FImGuiLabAdapter::GetFallbackScalarCount() const noexcept { return Impl->FallbackCount; }
EApplicationResult FImGuiLabAdapter::GetClipboardResult() const noexcept { return Impl->ClipboardResult; }
Stoner::Core::FString FImGuiLabAdapter::GetText() const { return Impl->Text.data(); }
}
