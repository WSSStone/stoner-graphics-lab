#include "FImGuiLabAdapter.h"
#include "FImGuiInputAdapter.h"
#include "FEmbeddedLabFont.h"
#include "Core/FPlatformProcess.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
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
    std::unique_ptr<FImGuiTextureAdapter> Textures;
    Stoner::RHI::ERHIResult TextureResult = Stoner::RHI::ERHIResult::Unsupported;
    Stoner::Core::uint64 TextureFrame = 0;
    FUILabCapture Capture;
    Stoner::Core::FString Clipboard;
    EApplicationResult ClipboardResult = EApplicationResult::Success;
    std::array<char, 65537> Text{};
    Stoner::Core::uint32 VertexCount = 0;
    Stoner::Core::uint64 FallbackCount = 0;
    Stoner::Core::uint64 DisplayGeneration = 0;
    bool bReady = false;
    bool bDrawReady = false;
    Stoner::Core::uint32 DrawableWidth = 0, DrawableHeight = 0;
    ~FImpl() { Textures.reset(); if (Context) ImGui::DestroyContext(Context); }
};

FImGuiLabAdapter::FImGuiLabAdapter() : Impl(std::make_unique<FImpl>()) {}
FImGuiLabAdapter::~FImGuiLabAdapter() = default;

EApplicationResult FImGuiLabAdapter::Initialize(FWindow& Window, FImGuiTextureAdapter::FPrepare PrepareTexture)
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
    if (PrepareTexture) Impl->Textures = std::make_unique<FImGuiTextureAdapter>(std::move(PrepareTexture));
    IO.BackendFlags = Impl->Textures ? (ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset) : ImGuiBackendFlags_None;
    IO.BackendRendererName = Impl->Textures ? "Stoner.Renderer" : nullptr;
    IO.Fonts->TexMaxWidth = IO.Fonts->TexMaxHeight = 2048;
    IO.BackendPlatformName = "Stoner.Application";
    ImFontConfig FontConfig;
    FontConfig.FontDataOwnedByAtlas = false;
    const auto Font = GetEmbeddedLabFont();
    if (Font.empty() || Font.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return EApplicationResult::ValidationFailed;
    IO.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(Font.data()),
        static_cast<int>(Font.size()), 16.0f, &FontConfig);
    if (!Impl->Textures)
    {
        unsigned char* Pixels = nullptr;
        int Width = 0, Height = 0;
        IO.Fonts->GetTexDataAsRGBA32(&Pixels, &Width, &Height);
        if (!Pixels || Width <= 0 || Height <= 0 || Width > 2048 || Height > 2048)
            return EApplicationResult::ValidationFailed;
    }
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
    const FWindowDisplayState& Display, double DeltaSeconds, bool bRenderEligible,
    std::span<const FLabControlSection> Sections,
    const std::function<bool(const Stoner::Core::FString&,const Stoner::Core::FString&)>& Invoke,
    bool bEditsEnabled)
{
    Impl->Capture = {};
    Impl->bDrawReady = false;
    Impl->VertexCount = 0;
    if (!Impl->bReady || !Impl->Context || !Impl->Window || !Impl->Window->IsActive()) return EApplicationResult::InvalidLifecycle;
    if (Events.size() > 4096 || Display.DisplayGeneration < Impl->DisplayGeneration ||
        !Display.IsValid() || !Display.DrawableExtent.IsPositive() ||
        !Display.LogicalExtent.IsPositive() || !std::isfinite(DeltaSeconds) || DeltaSeconds < 0)
        return EApplicationResult::InvalidInput;
    if (Impl->Textures && Impl->Textures->IsFailed()) return EApplicationResult::RuntimeUnavailable;
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
    // Dynamic atlases can load the bundled font's extended glyphs on demand.
    // The legacy CPU fixture remains restricted to its prebaked Latin range.
    Impl->FallbackCount = 0;
    const char* Cursor = Impl->Text.data();
    const char* End = Cursor + std::string_view(Cursor).size();
    while (Cursor < End)
    {
        unsigned int Scalar = 0;
        const int Bytes = ImTextCharFromUtf8(&Scalar, Cursor, End);
        if (Bytes <= 0) break;
        if ((!Impl->Textures && Scalar > 0xFF) || !ImGui::GetFont()->IsGlyphInFont(Scalar))
            ++Impl->FallbackCount;
        Cursor += Bytes;
    }
    if (Impl->FallbackCount) ImGui::TextUnformatted("Bundled font: some characters use replacement glyphs.");
    if (Impl->ClipboardResult != EApplicationResult::Success)
        ImGui::TextUnformatted("Clipboard unavailable or rejected; text preserved.");
    const bool HideRequested = ImGui::Button("Hide UI (F1)");
    for (const auto& Section : Sections)
    {
        ImGui::PushID(Section.Id.CStr());
        // Visible labels are separate from IDs; feature labels cannot inject
        // ImGui's hidden-identity suffix into command dispatch.
        if (ImGui::TreeNodeEx("section",ImGuiTreeNodeFlags_DefaultOpen,"%s",Section.Title.CStr()))
        {
            ImGui::BeginDisabled(!bEditsEnabled || !Invoke);
            const auto Control = [&](const auto& Item) {
                ImGui::PushID(Item.Id.CStr());
                if (ImGui::Button(Item.Label.CStr()) && Invoke) (void)Invoke(Section.Id,Item.Id);
                ImGui::PopID();
            };
            for (const auto& Command : Section.Commands) Control(Command);
            for (const auto& View : Section.DebugViews) Control(View);
            ImGui::EndDisabled();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    const bool Active = ImGui::IsAnyItemActive();
    Impl->Capture = {};
    Impl->Capture.bHideUIRequested = HideRequested;
    Impl->Capture.bVisibilityChanged = HideRequested;
    Impl->Capture.bKeyboard = TextActive || (Active && IO.WantCaptureKeyboard);
    Impl->Capture.bTextEditing = TextActive;
    bool MouseDown = false;
    for (bool Down : IO.MouseDown) MouseDown |= Down;
    Impl->Capture.bPointer = IO.WantCaptureMouse && (MouseDown || Active);
    Impl->Capture.bScroll = IO.WantCaptureMouse && (IO.MouseWheel != 0 || IO.MouseWheelH != 0);
    ImGui::End();
    ImGui::Render();
    Impl->DisplayGeneration = Display.DisplayGeneration;
    if (Impl->Textures)
    {
        auto* Data = ImGui::GetDrawData();
        const auto Now = static_cast<Stoner::Core::uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        Impl->TextureResult = Impl->Textures->Process(Data->Textures
            ? std::span<ImTextureData* const>(Data->Textures->Data, static_cast<std::size_t>(Data->Textures->Size))
            : std::span<ImTextureData* const>{}, ++Impl->TextureFrame, bRenderEligible, Now);
        if (Impl->TextureResult != Stoner::RHI::ERHIResult::Success)
            return EApplicationResult::RuntimeUnavailable;
    }
    Impl->VertexCount = static_cast<Stoner::Core::uint32>(ImGui::GetDrawData()->TotalVtxCount);
    Impl->DrawableWidth = Display.DrawableExtent.Width;
    Impl->DrawableHeight = Display.DrawableExtent.Height;
    Impl->bDrawReady = true;
    return EApplicationResult::Success;
}

void FImGuiLabAdapter::Suspend() noexcept
{
    Impl->Capture = {}; Impl->bDrawReady = false; Impl->VertexCount = 0;
    Impl->Input = {};
    if (!Impl->Context) return;
    ImGui::SetCurrentContext(Impl->Context);
    auto& IO = ImGui::GetIO();
    IO.ClearEventsQueue(); IO.ClearInputKeys(); IO.ClearInputMouse();
    ImGui::ClearActiveID();
}

Stoner::RHI::ERHIResult FImGuiLabAdapter::ExtractSnapshot(const FAcquireTexture& AcquireTexture,
    Stoner::Renderer::FUIDrawSnapshot& OutSnapshot) const
{
    if (!Impl->bDrawReady || !Impl->Textures || !AcquireTexture ||
        OutSnapshot.GetDisplayGeneration() != Impl->DisplayGeneration)
        return Stoner::RHI::ERHIResult::InvalidState;
    ImGui::SetCurrentContext(Impl->Context);
    return FImGuiDrawAdapter::Extract(*ImGui::GetDrawData(),
        [&](Stoner::Core::uint64 Token) { return AcquireTexture(Impl->Textures->Resolve(Token)); },
        Impl->DrawableWidth, Impl->DrawableHeight, OutSnapshot);
}

const char* FImGuiLabAdapter::GetTextureDiagnostic() const noexcept { return Impl->Textures ? Impl->Textures->GetDiagnostic() : "ui-textures-unavailable"; }
Stoner::RHI::ERHIResult FImGuiLabAdapter::GetTextureResult() const noexcept { return Impl->TextureResult; }
FUILabCapture FImGuiLabAdapter::GetCapture() const noexcept { return Impl->Capture; }
Stoner::Core::uint32 FImGuiLabAdapter::GetVertexCount() const noexcept { return Impl->VertexCount; }
Stoner::Core::uint64 FImGuiLabAdapter::GetFallbackScalarCount() const noexcept { return Impl->FallbackCount; }
EApplicationResult FImGuiLabAdapter::GetClipboardResult() const noexcept { return Impl->ClipboardResult; }
Stoner::Core::FString FImGuiLabAdapter::GetText() const { return Impl->Text.data(); }
}
