#include "FImGuiTextureAdapter.h"
#include "FImGuiLabAdapter.h"
#include "FUITextureRegistry.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <array>
#include <iostream>
#include <cmath>

int RunApplicationUITextureTests()
{
    using namespace Stoner;
    using namespace Stoner::Application;
    using namespace Stoner::Renderer;
    using namespace Stoner::RHI;
    int Failed = 0;
    auto Check = [&](bool OK, const char* Name)
    {
        std::cout << (OK ? "[PASS] " : "[FAIL] ") << Name << '\n';
        if (!OK) ++Failed;
    };
    ImGui::CreateContext();
    {
        FUITextureRequest Last;
        Core::uint64 Generation = 0;
        ERHIResult Next = ERHIResult::Success;
        FImGuiTextureAdapter Adapter([&](const FUITextureRequest& R)
        {
            Last = R;
            if (Next == ERHIResult::NotReady)
                return FUITextureResult{R.RequestId, Next,
                    R.Operation == EUITextureOperation::Destroy ? EUITextureState::Retiring : EUITextureState::Requested,
                    R.TextureId, "pending"};
            if (Next != ERHIResult::Success)
                return FUITextureResult{R.RequestId, Next, EUITextureState::Rejected, {}, "failed"};
            return FUITextureResult{R.RequestId, ERHIResult::Success,
                R.Operation == EUITextureOperation::Destroy ? EUITextureState::Destroyed : EUITextureState::Prepared,
                R.Operation == EUITextureOperation::Destroy ? R.TextureId : FUITextureId{1, ++Generation}, "prepared"};
        });
        ImTextureData Texture;
        Texture.Create(ImTextureFormat_Alpha8, 2, 2);
        Texture.Pixels[0] = 0; Texture.Pixels[1] = 128;
        Texture.Pixels[2] = 255; Texture.Pixels[3] = 17;
        std::array<ImTextureData*, 1> Textures{&Texture};
        Check(Adapter.Process(Textures, 1, false, 0) == ERHIResult::NotReady && Last.RequestId == 0,
            "minimized ImGui texture requests remain untouched");
        Check(Adapter.Process(Textures, 2, true, 1) == ERHIResult::Success &&
            Texture.Status == ImTextureStatus_OK && Texture.GetTexID() != ImTextureID_Invalid &&
            Adapter.Resolve(Texture.GetTexID()) == FUITextureId{1, 1},
            "successful preparation acknowledges ImGui create with an opaque engine identity");
        Check(Last.PixelBytes.size() == 16 && Last.PixelBytes[0] == 255 && Last.PixelBytes[3] == 0 &&
            Last.PixelBytes[7] == 128 && Last.PixelBytes[11] == 255 && Last.PixelBytes[15] == 17 &&
            Last.ColorDomain == EUITextureColorDomain::AlphaCoverage,
            "alpha-only atlas normalizes to white RGB and unchanged linear coverage");
        const auto OldToken = Texture.GetTexID();
        const auto PreparedRequest = Last.RequestId;
        Check(Adapter.Process(Textures, 3, false, 2) == ERHIResult::Success &&
            Last.RequestId == PreparedRequest && Texture.GetTexID() == OldToken,
            "busy frame slots keep prepared UI textures drawable without another upload");
        Texture.Pixels[3] = 99; Texture.SetStatus(ImTextureStatus_WantUpdates);
        Check(Adapter.Process(Textures, 3, false, 2) == ERHIResult::NotReady &&
            Last.RequestId == PreparedRequest,
            "busy frame slots still defer changed atlas data without acknowledging it");
        Next = ERHIResult::NotReady;
        Check(Adapter.Process(Textures, 3, true, 2) == ERHIResult::NotReady &&
            Texture.Status == ImTextureStatus_WantUpdates && Texture.GetTexID() == OldToken,
            "deferred update never acknowledges or replaces the current texture ID");
        Next = ERHIResult::Success;
        Check(Adapter.Process(Textures, 4, true, 3) == ERHIResult::Success &&
            Last.Operation == EUITextureOperation::Update && Last.ExpectedGeneration == 1 &&
            Last.PixelBytes[15] == 99 && Last.PixelBytes[7] == 128 &&
            Adapter.Resolve(Texture.GetTexID()) == FUITextureId{1, 2} &&
            !Adapter.Resolve(OldToken).IsValid(),
            "full-image update publishes a new generation and invalidates the old adapter token");
        Texture.WantDestroyNextFrame = true;
        Texture.SetStatus(ImTextureStatus_WantDestroy); Next = ERHIResult::NotReady;
        Check(Adapter.Process(Textures, 5, true, 4) == ERHIResult::NotReady &&
            Texture.Status == ImTextureStatus_WantDestroy,
            "ImGui destroy acknowledgement waits for Renderer retirement");
        Next = ERHIResult::Success;
        Check(Adapter.Process(Textures, 6, true, 5) == ERHIResult::Success &&
            Texture.Status == ImTextureStatus_Destroyed && Texture.GetTexID() == ImTextureID_Invalid,
            "completed retirement acknowledges ImGui destruction and clears native token");
    }
    {
        Core::uint32 Calls = 0;
        FImGuiTextureAdapter Adapter([&](const FUITextureRequest& R)
        {
            ++Calls;
            return FUITextureResult{R.RequestId, ERHIResult::NotReady, EUITextureState::Requested, {}, "pending"};
        });
        ImTextureData Texture; Texture.Create(ImTextureFormat_RGBA32, 1, 1);
        std::array<ImTextureData*, 1> Textures{&Texture};
        bool Bounded = true;
        for (Core::uint64 Frame = 1; Frame < 120; ++Frame)
            Bounded &= Adapter.Process(Textures, Frame, true, Frame) == ERHIResult::NotReady;
        Check(Bounded && Calls == 119 && Adapter.Process(Textures, 120, true, 120) == ERHIResult::Unavailable &&
            std::string_view(Adapter.GetDiagnostic()) == "ui-texture-retry-frame-limit",
            "ImGui retry exhausts after 120 eligible frames");
        const auto Before = Calls;
        Check(Adapter.Process(Textures, 121, true, 121) == ERHIResult::Unavailable && Calls == Before,
            "exhausted texture preparation stays disabled until explicit reinitialization");
    }
    {
        FImGuiTextureAdapter Adapter([](const FUITextureRequest& R)
        { return FUITextureResult{R.RequestId, ERHIResult::NotReady, EUITextureState::Requested, {}, "pending"}; });
        ImTextureData Texture; Texture.Create(ImTextureFormat_RGBA32, 1, 1);
        std::array<ImTextureData*, 1> Textures{&Texture};
        Check(Adapter.Process(Textures, 1, true, 100) == ERHIResult::NotReady &&
            Adapter.Process(Textures, 2, false, 5099) == ERHIResult::NotReady &&
            Adapter.Process(Textures, 3, true, 5100) == ERHIResult::Unavailable &&
            std::string_view(Adapter.GetDiagnostic()) == "ui-texture-retry-deadline",
            "five-second retry deadline wins while minimized polls issue no uploads");
    }
    {
        Core::uint32 Calls = 0;
        FImGuiTextureAdapter Adapter([&](const FUITextureRequest& R)
        {
            ++Calls;
            return FUITextureResult{R.RequestId, ERHIResult::Success,
                R.Operation == EUITextureOperation::Destroy ? EUITextureState::Destroyed : EUITextureState::Prepared,
                R.Operation == EUITextureOperation::Destroy ? R.TextureId : FUITextureId{R.LogicalSlot, 1}, "prepared"};
        });
        std::array<ImTextureData, 65> Storage;
        std::array<ImTextureData*, 65> Textures;
        for (std::size_t I = 0; I < Storage.size(); ++I)
        {
            Storage[I].Create(ImTextureFormat_RGBA32, 1, 1);
            Textures[I] = &Storage[I];
        }
        Check(Adapter.Process(Textures, 1, true, 0) == ERHIResult::NotReady && Calls == 64 &&
            Storage[64].Status == ImTextureStatus_WantCreate,
            "a burst prepares at most 64 requests and leaves remaining upstream work pending");
        Check(Adapter.Process(Textures, 1, true, 1) == ERHIResult::NotReady && Calls == 64,
            "duplicate frame identity cannot consume another texture request budget");
        Check(Adapter.Process(Textures, 2, true, 2) == ERHIResult::Success && Calls == 65,
            "next eligible frame completes the bounded request burst coherently");
    }
    {
        Core::uint32 Calls = 0;
        FImGuiTextureAdapter Adapter([&](const FUITextureRequest& R)
        { ++Calls; return FUITextureResult{R.RequestId, ERHIResult::Failed, EUITextureState::Rejected, {}, "failed"}; });
        ImTextureData Texture; Texture.Create(ImTextureFormat_RGBA32, 1, 1);
        Texture.Width = 2049;
        std::array<ImTextureData*, 1> Textures{&Texture};
        Check(Adapter.Process(Textures, 1, true, 0) == ERHIResult::Unavailable && Calls == 0 &&
            Texture.Status == ImTextureStatus_WantCreate && Texture.GetTexID() == ImTextureID_Invalid,
            "oversized upstream extent fails before pixel access or any acknowledgement");
    }
    {
        Core::uint32 Calls = 0;
        FImGuiTextureAdapter Adapter([&](const FUITextureRequest& R)
        { ++Calls; return FUITextureResult{R.RequestId, ERHIResult::Failed, EUITextureState::Rejected, {}, "failed"}; });
        ImTextureData Texture; Texture.Create(ImTextureFormat_RGBA32, 1, 1);
        std::array<ImTextureData*, 2> Textures{&Texture, &Texture};
        Check(Adapter.Process(Textures, 1, true, 0) == ERHIResult::Unavailable && Calls == 0,
            "duplicate upstream texture pointers fail whole-frame preparation before Renderer calls");
    }
    {
        FImGuiTextureAdapter Adapter([](const FUITextureRequest& R)
        { return FUITextureResult{R.RequestId, ERHIResult::Success, EUITextureState::Prepared, {2, 1}, "wrong slot"}; });
        ImTextureData Texture; Texture.Create(ImTextureFormat_RGBA32, 1, 1);
        std::array<ImTextureData*, 1> Textures{&Texture};
        Check(Adapter.Process(Textures, 1, true, 0) == ERHIResult::Unavailable &&
            Texture.Status == ImTextureStatus_WantCreate && Texture.GetTexID() == ImTextureID_Invalid,
            "mismatched Renderer identity cannot be acknowledged as a successful create");
    }
    {
        FUITextureRequest Last;
        FImGuiTextureAdapter Adapter([&](const FUITextureRequest& R)
        {
            Last = R;
            return FUITextureResult{R.RequestId, ERHIResult::Success, EUITextureState::Prepared, {1, 1}, "prepared"};
        });
        ImTextureData Texture; Texture.Create(ImTextureFormat_RGBA32, 1, 1);
        Texture.UseColors = true;
        Texture.Pixels[0] = 128; Texture.Pixels[1] = 64; Texture.Pixels[2] = 32; Texture.Pixels[3] = 17;
        std::array<ImTextureData*, 1> Textures{&Texture};
        Check(Adapter.Process(Textures, 1, true, 0) == ERHIResult::Success &&
            Last.Format == ERHIFormat::R8G8B8A8_sRGB && Last.ColorDomain == EUITextureColorDomain::SRGBRec709 &&
            Last.PixelBytes == Core::TArray<Core::uint8>({128, 64, 32, 17}),
            "colored atlas preserves encoded RGB bytes and requests hardware sRGB sampling");
    }
    ImGui::DestroyContext();
    {
        auto Device = Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
        Backend::Vulkan::FVulkanInstanceDesc Desc;
        Desc.RuntimeMode = Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
        const auto Initialized = Device->Initialize(Desc);
        Check(Initialized == ERHIResult::Success, "dynamic atlas fixture device initializes");
        if (Initialized != ERHIResult::Success) return Failed;
        FWindow Window;
        Check(Window.Create({}) == EApplicationResult::Success, "dynamic atlas fixture window initializes");
        FUITextureRegistry Registry(Device);
        Core::uint32 Creates = 0, Updates = 0;
        {
            FImGuiLabAdapter UI;
            Check(UI.Initialize(Window, [&](const FUITextureRequest& Request)
                {
                    if (Request.Operation == EUITextureOperation::Create) ++Creates;
                    if (Request.Operation == EUITextureOperation::Update) ++Updates;
                    return Registry.Prepare(Request);
                }) == EApplicationResult::Success &&
                (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0,
                "real atlas advertises texture support only with a Renderer request service");
            const auto Display = Window.GetDisplayState();
            Registry.BeginEligibleFrame(1, true);
            Check(UI.Frame({}, Display, 1.0 / 60.0) == EApplicationResult::Success && Creates > 0 &&
                UI.GetTextureResult() == ERHIResult::Success && UI.GetVertexCount() > 0,
                "real dynamic font frame prepares and acknowledges its Renderer atlas generation");
            FUIDrawSnapshot Copied(1, 1, 1, Display.DisplayGeneration);
            Check(UI.ExtractSnapshot([&](FUITextureId Id) { return Registry.Acquire(Id); }, Copied) ==
                ERHIResult::Success && Copied.IsPublished() && !Copied.GetTextureLeases().empty(),
                "real ImGui context exports an immutable leased draw snapshot");
            const auto CopiedVertexCount = Copied.GetVertices().size();
            const auto CopiedTexture = Copied.GetTextureLeases().empty() ? FUITextureId{} : Copied.GetTextureLeases()[0].GetId();
            Registry.BeginEligibleFrame(2, true);
            (void)UI.Frame({FInputEvent::PointerMove(60, 70), FInputEvent::MouseDown(EMouseButton::Left)}, Display, 1.0 / 60.0);
            Registry.BeginEligibleFrame(3, true);
            Check(UI.Frame({FInputEvent::MouseUp(EMouseButton::Left), FInputEvent::Text(0x00E9)}, Display,
                1.0 / 60.0) == EApplicationResult::Success && Updates > 0 && UI.GetFallbackScalarCount() == 0,
                "new real font glyphs flow through full-image copy-on-write atlas updates");
            Check(Copied.GetVertices().size() == CopiedVertexCount && CopiedTexture.IsValid() &&
                Copied.GetTextureLeases()[0].GetId() == CopiedTexture && Registry.GetStatistics().Generations >= 2,
                "subsequent real font frame preserves the queued snapshot and its old atlas");
            FUIDrawSnapshot Stale(1, 4, 1, Display.DisplayGeneration + 1);
            Check(UI.ExtractSnapshot([&](FUITextureId Id) { return Registry.Acquire(Id); }, Stale) ==
                ERHIResult::InvalidState && !Stale.IsPublished(),
                "context extraction rejects a mismatched display generation");
            const auto RequestsBeforeBusy = Creates + Updates;
            Registry.BeginEligibleFrame(4, false);
            const auto Busy = UI.Frame({FInputEvent::Text('z')},Display,1.0/60.0,false);
            FUIDrawSnapshot BusyDraw(1,5,1,Display.DisplayGeneration);
            Check(Busy == EApplicationResult::RuntimeUnavailable && UI.GetTextureResult() == ERHIResult::NotReady &&
                UI.GetCapture().bTextEditing && UI.GetText().View().find('z') != std::string_view::npos &&
                Creates + Updates == RequestsBeforeBusy &&
                UI.ExtractSnapshot([&](FUITextureId Id) { return Registry.Acquire(Id); },BusyDraw) == ERHIResult::InvalidState,
                "busy render interval services active UI text without texture requests or publishing stale draws");
            UI.Suspend();
            Check(!UI.GetCapture().bTextEditing && UI.GetVertexCount() == 0 &&
                ImGui::GetCurrentContext()->InputEventsQueue.empty() && ImGui::GetCurrentContext()->ActiveId == 0,
                "hidden/minimized UI clears active input and queued events without destroying font ownership");
            Registry.BeginEligibleFrame(5,true);
            Check(UI.Frame({},Display,1.0/60.0) == EApplicationResult::Success && UI.GetVertexCount() > 0,
                "UI resumes eligible texture preparation after a suspended interval");
            FLabRuntimeInfo Runtime;
            Runtime.Workload="Diagnostic fixture";
            FLabSettingsSnapshot Selection;
            Selection.RequestedProfileId=Selection.EffectiveProfileId="Sdr.sRGB.v1";
            Selection.DebugBypass.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
            Selection.DebugBypass.StageName="ManualExposure";
            Selection.DebugBypass.SourceDomain=ERenderGraphColorDomain::SceneLinearRec709D65;
            Selection.DebugBypass.VisualizationMinimum=2;
            Selection.DebugBypass.VisualizationMaximum=8;
            FLabSettingsCapabilities Caps;
            Caps.DisplayGeneration=Display.DisplayGeneration;
            Caps.Outputs={{"Sdr.sRGB.v1",100,100}};
            Caps.DebugStages={{"ManualExposure",ERenderGraphColorDomain::SceneLinearRec709D65,{}}};
            Core::uint32 Edits=0;
            const auto Edit=[&](const FLabSettingsSnapshot&) { ++Edits; return true; };
            Core::uint64 ServiceFrame=5;
            const auto DiagnosticFrame=[&]() {
                Registry.BeginEligibleFrame(++ServiceFrame,true);
                if (UI.Frame({},Display,1.0/60.0,true,{}, {},true,&Runtime,nullptr,
                    &Selection,nullptr,&Selection,nullptr,Edit,&Caps)!=EApplicationResult::Success) return -1;
                FUIDrawSnapshot Draw(1,ServiceFrame,1,Display.DisplayGeneration);
                if (UI.ExtractSnapshot([&](FUITextureId Id) { return Registry.Acquire(Id); },Draw)!=ERHIResult::Success) return -1;
                int Images=0;
                for (const auto& Command : Draw.GetCommands()) Images+=Command.bDiagnosticWidget ? 1 : 0;
                return Images;
            };
            Check(DiagnosticFrame()==1 && Edits==0,
                "visible real diagnostic panel emits exactly one unresolved GPU image without an edit");
            auto* Panel=ImGui::FindWindowByName("Rendering Lab");
            const auto Header=Panel->GetID("Diagnostic view");
            // Exercise the persisted state read by the real collapsing header;
            // no renderer visibility flag or synthetic draw packet is substituted.
            Panel->StateStorage.SetInt(Header,0);
            Check(DiagnosticFrame()==0 && Edits==0 && Selection.DebugBypass.VisualizationMinimum==2 &&
                Selection.DebugBypass.VisualizationMaximum==8,
                "collapsed diagnostic panel emits no image request and preserves selected range");
            Panel->StateStorage.SetInt(Header,1);
            Check(DiagnosticFrame()==1 && Edits==0,
                "reopened diagnostic panel restores its GPU image request without a settings edit");
            Selection.DebugBypass.Mode=EOutputTransformDebugBypassMode::HDRPreservingReadback;
            Check(DiagnosticFrame()==0 && Edits==0,
                "real numeric diagnostic panel publishes UI without an image request or automatic action");
            auto ScaledDisplay=Display;
            ScaledDisplay.LogicalExtent={640,360};
            bool ScalesValid=true,CacheBounded=true,DensityMatches=true;
            for (int Step=0;Step<=35;++Step)
            {
                const float Scale=0.5f+0.1f*Step;
                ScaledDisplay.ContentScale={Scale,Scale};
                ScaledDisplay.DrawableExtent={static_cast<Core::uint32>(640*Scale),static_cast<Core::uint32>(360*Scale)};
                ScaledDisplay.FramebufferScale={static_cast<float>(ScaledDisplay.DrawableExtent.Width)/640,
                    static_cast<float>(ScaledDisplay.DrawableExtent.Height)/360};
                ++ScaledDisplay.DisplayGeneration;
                Registry.BeginEligibleFrame(++ServiceFrame,true);
                ScalesValid &= UI.Frame({},ScaledDisplay,1.0/60.0)==EApplicationResult::Success;
                auto* Builder=ImGui::GetIO().Fonts->Builder;
                CacheBounded &= Builder && Builder->BakedPool.Size-Builder->BakedDiscardedCount<=16;
                bool UsedDensity=false;
                if (Builder) for (int I=0;I<Builder->BakedPool.Size;++I)
                {
                    const auto& Baked=Builder->BakedPool[I];
                    UsedDensity |= !Baked.WantDestroy && Baked.LastUsedFrame==Builder->FrameCount &&
                        std::abs(Baked.RasterizerDensity-ScaledDisplay.FramebufferScale.X)<0.001f;
                }
                DensityMatches &= UsedDensity;
            }
            Check(ScalesValid,"dynamic fonts prepare scales from 0.5 through 4 without disabling UI");
            Check(CacheBounded,"dynamic font raster cache never exceeds sixteen live sizes");
            Check(DensityMatches,"dynamic font raster density follows the drawable ratio exactly once");
        }
        Registry.Poll();
        Check(Registry.GetStatistics().Generations == 0,
            "context shutdown cancels CPU requests and retires unleased Renderer atlas generations");
        (void)Window.Destroy();
        (void)Device->Shutdown();
    }
    return Failed;
}
