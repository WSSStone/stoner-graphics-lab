#include "FImGuiLabAdapter.h"
#include "Application/FInteractiveLabSession.h"
#include "FImGuiInputAdapter.h"
#include "FEmbeddedLabFont.h"
#include "Core/FPlatformProcess.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <sstream>
#include <locale>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace Stoner::Application
{
namespace
{
bool ExactValue(const char* Label, float& Value, std::array<char,64>& Buffer)
{
    if (ImGui::GetActiveID() != ImGui::GetID(Label))
        std::snprintf(Buffer.data(),Buffer.size(),"%.9g",static_cast<double>(Value));
    if (!ImGui::InputText(Label,Buffer.data(),Buffer.size(),ImGuiInputTextFlags_EnterReturnsTrue)) return false;
    const char* Begin=Buffer.data();
    if (*Begin=='+') ++Begin;
    float Parsed=0;
    std::istringstream Input(Begin);
    Input.imbue(std::locale::classic());
    Input >> std::noskipws >> Parsed;
    if (Input.fail() || !Input.eof() || !std::isfinite(Parsed)) return false;
    Value=Parsed;
    return true;
}
}
struct FImGuiLabAdapter::FImpl
{
    ImGuiContext* Context = nullptr;
    FWindow* Window = nullptr;
    FImGuiInputAdapter Input;
    std::unique_ptr<FImGuiTextureAdapter> Textures;
    Stoner::RHI::ERHIResult TextureResult = Stoner::RHI::ERHIResult::Unsupported;
    Stoner::Core::uint64 TextureFrame = 0;
    FUILabCapture Capture;
    std::array<char,64> ExactSpeed{}, ExactFov{}, ExactExposure{}, ExactBrightness{};
    FImGuiDiagnosticRange DiagnosticRange;
    Stoner::Core::FString Clipboard;
    EApplicationResult ClipboardResult = EApplicationResult::Success;
    std::array<char, 65537> Text{};
    std::array<char, 4097> PresetInput{};
    std::array<char, 129> PresetFilename{'l','a','b','-','p','r','e','s','e','t','.','j','s','o','n',0};
    Stoner::Core::FString PresetActionStatus;
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
    bool bEditsEnabled, const FLabRuntimeInfo* Runtime, const FFreeCameraState* Camera,
    const FLabSettingsSnapshot* Requested, const FLabSettingsSnapshot* Pending,
    const FLabSettingsSnapshot* Effective, const Stoner::Core::FString* SettingsFailure,
    const std::function<bool(const FLabSettingsSnapshot&)>& EditSettings,
    const FLabSettingsCapabilities* Capabilities,
    const std::function<bool(float,float)>& EditNavigation,
    const std::function<bool()>& ResetCamera, const FLabPresetActions* Presets,
    const FLabSessionStatistics* Statistics)
{
    Impl->Capture = {};
    Impl->bDrawReady = false;
    Impl->DiagnosticRange = {};
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
    if (Impl->Textures && IO.Fonts->Builder &&
        IO.Fonts->Builder->BakedPool.Size-IO.Fonts->Builder->BakedDiscardedCount>=16)
    {
        // Only unused CPU bakes are discarded. Published draw/texture snapshots
        // own copied geometry and GPU generations independently of these bakes.
        ImFontAtlasBuildDiscardBakes(IO.Fonts,1);
    }
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(Runtime ? std::max(128.0f,std::min(360.0f,IO.DisplaySize.x-32)) : 360.0f, Runtime ? std::max(120.0f,std::min(680.0f,IO.DisplaySize.y-32)) : 220.0f), ImGuiCond_FirstUseEver);
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
    if (Runtime)
    {
        if (Requested && Effective && EditSettings && Capabilities &&
            ImGui::CollapsingHeader("Diagnostic view",ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (Effective->DebugBypass.Mode==Stoner::Renderer::EOutputTransformDebugBypassMode::BoundedVisualization && Impl->Textures)
            {
                auto* List=ImGui::GetWindowDrawList();
                const auto First=static_cast<Stoner::Core::uint32>(List->IdxBuffer.Size);
                const float Width=std::max(1.0f,std::min(384.0f,ImGui::GetContentRegionAvail().x));
                ImGui::Image(IO.Fonts->TexRef,ImVec2(Width,Width*Display.DrawableExtent.Height/Display.DrawableExtent.Width));
                const auto Count=static_cast<Stoner::Core::uint32>(List->IdxBuffer.Size)-First;
                if (Count) Impl->DiagnosticRange={List,First,Count};
            }
            auto Candidate=*Requested;
            ImGui::BeginDisabled(!bEditsEnabled);
            const char* Mode=Candidate.DebugBypass.Mode==Stoner::Renderer::EOutputTransformDebugBypassMode::Disabled
                ? "Disabled" : Candidate.DebugBypass.Mode==Stoner::Renderer::EOutputTransformDebugBypassMode::BoundedVisualization
                ? "Image" : "NumericReadback";
            const auto SetMode=[&](Stoner::Renderer::EOutputTransformDebugBypassMode Value) {
                if (Value==Stoner::Renderer::EOutputTransformDebugBypassMode::Disabled) Candidate.DebugBypass={};
                else
                {
                    Candidate.DebugBypass.Mode=Value;
                    if (Candidate.DebugBypass.StageName.IsEmpty())
                    { Candidate.DebugBypass.StageName="SceneColorHandoff";
                      Candidate.DebugBypass.SourceDomain=Stoner::Renderer::ERenderGraphColorDomain::SceneLinearRec709D65; }
                }
                (void)EditSettings(Candidate);
            };
            if (ImGui::BeginCombo("Diagnostic mode",Mode))
            {
                if (ImGui::Selectable("Disabled")) SetMode(Stoner::Renderer::EOutputTransformDebugBypassMode::Disabled);
                if (ImGui::Selectable("Image")) SetMode(Stoner::Renderer::EOutputTransformDebugBypassMode::BoundedVisualization);
                if (ImGui::Selectable("NumericReadback")) SetMode(Stoner::Renderer::EOutputTransformDebugBypassMode::HDRPreservingReadback);
                ImGui::EndCombo();
            }
            ImGui::BeginDisabled(Candidate.DebugBypass.Mode==Stoner::Renderer::EOutputTransformDebugBypassMode::Disabled);
            if (ImGui::BeginCombo("Diagnostic stage",Candidate.DebugBypass.StageName.CStr()))
            {
                for (const auto& Stage : Capabilities->DebugStages)
                    if ((Stage.ProfileId.IsEmpty() || Stage.ProfileId==Effective->EffectiveProfileId) &&
                        ImGui::Selectable(Stage.Name.CStr(),Candidate.DebugBypass.StageName==Stage.Name))
                    { Candidate.DebugBypass.StageName=Stage.Name; Candidate.DebugBypass.SourceDomain=Stage.Domain; (void)EditSettings(Candidate); }
                ImGui::EndCombo();
            }
            const bool Minimum=ImGui::InputFloat("Range minimum",&Candidate.DebugBypass.VisualizationMinimum);
            const bool Maximum=ImGui::InputFloat("Range maximum",&Candidate.DebugBypass.VisualizationMaximum);
            if (Minimum || Maximum) (void)EditSettings(Candidate);
            ImGui::EndDisabled(); ImGui::EndDisabled();
            ImGui::TextUnformatted("Diagnostic only. Selection does not request a capture.");
        }
        if (ImGui::CollapsingHeader("Loaded scene",ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Workload: %s",Runtime->Workload.CStr());
            ImGui::TextWrapped("Root: %s",Runtime->RootIdentity.CStr());
            ImGui::TextWrapped("Cooked generation: %s",Runtime->CookedGeneration.CStr());
        }
        if (ImGui::CollapsingHeader("Navigation",ImGuiTreeNodeFlags_DefaultOpen) && Camera)
        {
            ImGui::Text("Position: %.3f, %.3f, %.3f",Camera->Position.X,Camera->Position.Y,Camera->Position.Z);
            ImGui::Text("Yaw / pitch: %.1f / %.1f deg",Camera->YawRadians*57.2957795f,Camera->PitchRadians*57.2957795f);
            ImGui::Text("Speed: %.3f units/s [0.01, 100]",Camera->MovementSpeed);
            ImGui::Text("Vertical FOV: %.1f deg [20, 90]",Camera->VerticalFovRadians*57.2957795f);
            ImGui::BeginDisabled(!bEditsEnabled);
            if (EditNavigation)
            {
                float Speed=Camera->MovementSpeed;
                float Fov=Stoner::Core::FMath::RadiansToDegrees(Camera->VerticalFovRadians);
                bool SpeedEdited=ImGui::SliderFloat("Movement speed",&Speed,0.01f,100.0f,"%.2f",ImGuiSliderFlags_Logarithmic);
                SpeedEdited |= ExactValue("Exact speed (Enter)",Speed,Impl->ExactSpeed);
                bool FovEdited=ImGui::SliderFloat("Vertical FOV (degrees)",&Fov,20.0f,90.0f,"%.1f");
                FovEdited |= ExactValue("Exact FOV (Enter)",Fov,Impl->ExactFov);
                if (SpeedEdited || FovEdited) (void)EditNavigation(Speed,
                    FovEdited ? Stoner::Core::FMath::DegreesToRadians(Fov) : Camera->VerticalFovRadians);
            }
            if (ResetCamera && ImGui::Button("Reset camera")) (void)ResetCamera();
            ImGui::EndDisabled();
        }
        if (ImGui::CollapsingHeader("Output",ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Requested: %s",Requested ? Requested->RequestedProfileId.CStr() : Runtime->RequestedProfile.CStr());
            ImGui::TextWrapped("Pending: %s",Pending ? Pending->EffectiveProfileId.CStr() : "none");
            ImGui::TextWrapped("Effective: %s",Effective ? Effective->EffectiveProfileId.CStr() : Runtime->EffectiveProfile.CStr());
            ImGui::Text("Manual exposure: %.2f EV [-16, 16]",Effective ? Effective->ExposureStops : Runtime->ExposureStops);
            ImGui::TextWrapped("Active tone map / viewing transform: %s",Runtime->TransformVersion.CStr());
            if (Requested && EditSettings)
            {
                auto Candidate = *Requested;
                ImGui::BeginDisabled(!bEditsEnabled);
                const bool CurrentCapabilities = Capabilities && Capabilities->DisplayGeneration == Display.DisplayGeneration;
                ImGui::BeginDisabled(!CurrentCapabilities || Capabilities->Outputs.empty());
                if (ImGui::BeginCombo("Output profile",Candidate.RequestedProfileId.CStr()))
                {
                    if (CurrentCapabilities)
                        for (const auto& Output : Capabilities->Outputs)
                            if (ImGui::Selectable(Output.ProfileId.CStr(),Candidate.RequestedProfileId == Output.ProfileId))
                            { Candidate.RequestedProfileId = Output.ProfileId; (void)EditSettings(Candidate); }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
                if (!CurrentCapabilities) ImGui::TextUnformatted("Refreshing output capabilities");
                else if (Capabilities->Outputs.empty()) ImGui::TextUnformatted("No supported output; rendering paused");
                if (ImGui::SliderFloat("Exposure (EV)",&Candidate.ExposureStops,-16,16,"%.2f"))
                    (void)EditSettings(Candidate);
                if (ExactValue("Exact exposure (Enter)",Candidate.ExposureStops,Impl->ExactExposure))
                    (void)EditSettings(Candidate);
                const bool SDR = (Effective ? Effective->EffectiveProfileId : Candidate.RequestedProfileId).View().starts_with("Sdr.");
                ImGui::BeginDisabled(!SDR);
                if (ImGui::BeginCombo("SDR tone map",Candidate.SdrToneMapVersion.CStr()))
                {
                    for (const char* Version : {"Sdr.KhronosPbrNeutral.v1","Sdr.NarkowiczAcesFit.v1","Sdr.ExtendedReinhardRec709.v1"})
                        if (ImGui::Selectable(Version,Candidate.SdrToneMapVersion == Version))
                        { Candidate.SdrToneMapVersion = Version; (void)EditSettings(Candidate); }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
                if (ImGui::SliderFloat("UI brightness",&Candidate.UIWhiteMultiplier,0.25f,2.0f,"%.2fx"))
                    (void)EditSettings(Candidate);
                if (ExactValue("Exact brightness (Enter)",Candidate.UIWhiteMultiplier,Impl->ExactBrightness))
                    (void)EditSettings(Candidate);
                if (SDR && Candidate.UIWhiteMultiplier>1.0f)
                    ImGui::TextWrapped("SDR UI brightness above 1 may clip at the output limit.");
                if (!SDR) ImGui::TextUnformatted("HDR uses its viewing transform; SDR tone map is remembered.");
                ImGui::EndDisabled();
            }
            if (Effective)
            {
                ImGui::TextWrapped("Remembered SDR: %s",Effective->SdrToneMapVersion.CStr());
                ImGui::TextWrapped("Remembered HDR: %s",Effective->HdrViewingVersion.CStr());
                ImGui::TextWrapped("Debug stage: %s",Effective->DebugBypass.StageName.IsEmpty() ? "disabled" : Effective->DebugBypass.StageName.CStr());
            }
            else ImGui::TextUnformatted("Live settings/debug controls: not configured");
            if (SettingsFailure && !SettingsFailure->IsEmpty()) ImGui::TextWrapped("Settings failure: %s",SettingsFailure->CStr());
        }
        if (ImGui::CollapsingHeader("Diagnostics",ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Submitted / render complete: %llu / %llu",static_cast<unsigned long long>(Runtime->Submitted),static_cast<unsigned long long>(Runtime->RenderCompleted));
            ImGui::Text("Present queued: %llu",static_cast<unsigned long long>(Runtime->PresentQueued));
            ImGui::Text("UI / scene fallback: %llu / %llu",static_cast<unsigned long long>(Runtime->UIFrames),static_cast<unsigned long long>(Runtime->SceneFallbackFrames));
            ImGui::Text("Drawable: %u x %u",Display.DrawableExtent.Width,Display.DrawableExtent.Height);
            ImGui::Text("Display generation: %llu",static_cast<unsigned long long>(Display.DisplayGeneration));
            if (Statistics)
            {
                ImGui::Text("Input overflow intervals: %llu",static_cast<unsigned long long>(Statistics->InputOverflowIntervals));
                ImGui::Text("Cursor / UI enable failures: %llu / %llu",static_cast<unsigned long long>(Statistics->CursorCaptureFailures),static_cast<unsigned long long>(Statistics->UIEnableFailures));
                ImGui::Text("Capability pauses / evicted diagnostics: %llu / %llu",static_cast<unsigned long long>(Statistics->CapabilityPauses),static_cast<unsigned long long>(Statistics->DiagnosticEvictions));
            }
            ImGui::TextUnformatted("GPU time / full memory profiling: unavailable");
            if (!Runtime->Failure.IsEmpty()) ImGui::TextWrapped("Runtime failure: %s",Runtime->Failure.CStr());
        }
        if (ImGui::CollapsingHeader("Instructions"))
            ImGui::TextWrapped("WASD/QE: move. Shift: accelerate. Right mouse: look. Viewport wheel: FOV. F1: show/hide UI. Escape: cancel interaction. UI-owned input cannot move the camera. Preview is not an Accepted baseline; HDR export is diagnostic, not visual acceptance.");
    }
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
    if (Presets && ImGui::CollapsingHeader("Local presets"))
    {
        ImGui::TextUnformatted(Presets->bPending ? "Pending import; ordinary camera/output edits are disabled." : "No pending import.");
        ImGui::TextWrapped("A filename imports from the export directory. Paths with folders are relative to the working directory; absolute paths are also supported.");
        ImGui::BeginDisabled(Presets->bNativeActive || !Presets->Import);
        ImGui::InputText("Preset input path",Impl->PresetInput.data(),Impl->PresetInput.size());
        if (ImGui::Button("Import preset") && Presets->Import)
            Impl->PresetActionStatus = Presets->Import(Impl->PresetInput.data()) ? "Import accepted for preparation." : "Import rejected.";
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!Presets->bPending || Presets->bNativeActive || !Presets->Cancel);
        if (ImGui::Button("Cancel pending import") && Presets->Cancel)
            Impl->PresetActionStatus = Presets->Cancel() ? "Pending import cancelled." : "Cancellation unavailable.";
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!Presets->bCanExport || !Presets->Export);
        ImGui::InputText("Export filename",Impl->PresetFilename.data(),Impl->PresetFilename.size());
        if (ImGui::Button("Export new preset") && Presets->Export)
            Impl->PresetActionStatus = Presets->Export(Impl->PresetFilename.data(),false);
        ImGui::SameLine();
        if (ImGui::Button("Overwrite preset") && Presets->Export)
            Impl->PresetActionStatus = Presets->Export(Impl->PresetFilename.data(),true);
        ImGui::EndDisabled();
        if (!Presets->bCanExport) ImGui::TextWrapped("Export is unavailable until the export directory is configured and the session is stable.");
        if (Presets->bNativeActive) ImGui::TextUnformatted("Waiting for native output completion.");
        if (!Impl->PresetActionStatus.IsEmpty()) ImGui::TextWrapped("%s",Impl->PresetActionStatus.CStr());
        if (Presets->Failure && !Presets->Failure->IsEmpty()) ImGui::TextWrapped("%s",Presets->Failure->CStr());
    }
    const bool Active = ImGui::IsAnyItemActive();
    Impl->Capture = {};
    Impl->Capture.bHideUIRequested = HideRequested;
    Impl->Capture.bVisibilityChanged = HideRequested;
    Impl->Capture.bKeyboard = TextActive || IO.WantTextInput || (Active && IO.WantCaptureKeyboard);
    Impl->Capture.bTextEditing = TextActive || IO.WantTextInput;
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
        Impl->DrawableWidth, Impl->DrawableHeight, OutSnapshot,
        Impl->DiagnosticRange.IndexCount ? &Impl->DiagnosticRange : nullptr);
}

const char* FImGuiLabAdapter::GetTextureDiagnostic() const noexcept { return Impl->Textures ? Impl->Textures->GetDiagnostic() : "ui-textures-unavailable"; }
Stoner::RHI::ERHIResult FImGuiLabAdapter::GetTextureResult() const noexcept { return Impl->TextureResult; }
FUILabCapture FImGuiLabAdapter::GetCapture() const noexcept { return Impl->Capture; }
Stoner::Core::uint32 FImGuiLabAdapter::GetVertexCount() const noexcept { return Impl->VertexCount; }
Stoner::Core::uint64 FImGuiLabAdapter::GetFallbackScalarCount() const noexcept { return Impl->FallbackCount; }
EApplicationResult FImGuiLabAdapter::GetClipboardResult() const noexcept { return Impl->ClipboardResult; }
Stoner::Core::FString FImGuiLabAdapter::GetText() const { return Impl->Text.data(); }
}
