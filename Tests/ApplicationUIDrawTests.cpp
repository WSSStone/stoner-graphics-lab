#include "FImGuiDrawAdapter.h"
#include "FUITextureRegistry.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <iostream>
#include <limits>

namespace
{
bool CallbackCalled = false;
void ArbitraryCallback(const ImDrawList*, const ImDrawCmd*) { CallbackCalled = true; }
}
int RunApplicationUIDrawTests()
{
    using namespace Stoner;
    using namespace Stoner::Renderer;
    using namespace Stoner::Application;
    using namespace Stoner::RHI;
    int Failed = 0;
    auto Check = [&](bool OK, const char* Name)
    {
        std::cout << (OK ? "[PASS] " : "[FAIL] ") << Name << '\n';
        if (!OK) ++Failed;
    };
    auto Device = Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
    Backend::Vulkan::FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
    const bool Initialized = Device->Initialize(Desc) == ERHIResult::Success;
    Check(Initialized, "copied ImGui draw fixture initializes");
    if (!Initialized) return Failed;
    {
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1, true);
        FUITextureRequest Request;
        Request.RequestId = 1; Request.LogicalSlot = 1; Request.Width = Request.Height = 4;
        Request.Format = ERHIFormat::R8G8B8A8_UNorm;
        Request.ColorDomain = EUITextureColorDomain::AlphaCoverage;
        Request.PixelBytes.assign(64, 255);
        const auto Texture = Registry.Prepare(Request);
        const auto Resolve = [&](Core::uint64 Token)
        { return Token == 77 ? Registry.Acquire(Texture.TextureId) : FUITextureLease{}; };
        ImGui::CreateContext();
        {
            ImDrawList List(ImGui::GetDrawListSharedData());
            List.VtxBuffer.resize(4);
            for (int I = 0; I < 4; ++I)
                List.VtxBuffer[I] = {ImVec2(10.0f + I, 20.0f), ImVec2(0, 0), IM_COL32(128, 64, 32, 127)};
            List.IdxBuffer.resize(6);
            for (int I = 0; I < 3; ++I) List.IdxBuffer[I] = static_cast<ImDrawIdx>(999);
            for (int I = 0; I < 3; ++I) List.IdxBuffer[3 + I] = static_cast<ImDrawIdx>(I);
            List.CmdBuffer.resize(1);
            auto& Cmd = List.CmdBuffer[0];
            Cmd.ElemCount = 3; Cmd.IdxOffset = 3; Cmd.VtxOffset = 1;
            Cmd.ClipRect = ImVec4(8.5f, 18.5f, 30.1f, 40.1f);
            Cmd.TexRef = ImTextureRef(ImTextureID{77});
            ImDrawData Data;
            Data.Valid = true; Data.CmdLists.push_back(&List); Data.CmdListsCount = 1;
            Data.TotalVtxCount = 4; Data.TotalIdxCount = 6;
            Data.DisplayPos = ImVec2(10, 20); Data.DisplaySize = ImVec2(100, 80);
            Data.FramebufferScale = ImVec2(1.5f, 1.5f);
            FUIDrawSnapshot Snapshot(7, 9, 3, 4);
            Check(FImGuiDrawAdapter::Extract(Data, Resolve, 150, 120, Snapshot) == ERHIResult::Success &&
                Snapshot.IsPublished() && Snapshot.GetCommands()[0].FirstIndex == 3 &&
                Snapshot.GetCommands()[0].BaseVertex == 1 && Snapshot.GetTextureLeases().size() == 1 &&
                Snapshot.GetVertices()[0].PackedRGBA8 == 0x7F204080u,
                "ImGui draw extraction preserves offsets, canonical packed channels and a real generation lease");
            List.VtxBuffer[1].pos.x = 999; List.IdxBuffer[3] = 999;
            Check(Snapshot.GetVertices()[1].Position.X == 11 && Snapshot.GetIndices()[3] == 0,
                "queued snapshot cannot observe later ImGui buffer mutation");
            List.VtxBuffer[1].pos.x = 11; List.IdxBuffer[3] = 0;
            {
                ImDrawList Second(ImGui::GetDrawListSharedData());
                Second.VtxBuffer = List.VtxBuffer;
                Second.IdxBuffer = List.IdxBuffer;
                Second.CmdBuffer = List.CmdBuffer;
                Data.CmdLists.push_back(&Second); Data.CmdListsCount = 2;
                Data.TotalVtxCount = 8; Data.TotalIdxCount = 12;
                FUIDrawSnapshot Multi(7, 10, 3, 4);
                Check(FImGuiDrawAdapter::Extract(Data, Resolve, 150, 120, Multi) == ERHIResult::Success &&
                    Multi.GetCommands().size() == 2 && Multi.GetCommands()[1].FirstIndex == 9 &&
                    Multi.GetCommands()[1].BaseVertex == 5 && Multi.GetTextureLeases().size() == 1,
                    "multiple draw lists accumulate global offsets and deduplicate generation leases");
                Data.CmdLists.pop_back(); Data.CmdListsCount = 1;
                Data.TotalVtxCount = 4; Data.TotalIdxCount = 6;
            }
            {
                ImDrawList Merged(ImGui::GetDrawListSharedData());
                Merged.VtxBuffer = List.VtxBuffer;
                Merged.IdxBuffer.resize(12);
                for (int I=0; I<12; ++I) Merged.IdxBuffer[I]=static_cast<ImDrawIdx>(I%3);
                Merged.CmdBuffer = List.CmdBuffer;
                Merged.CmdBuffer[0].IdxOffset=0; Merged.CmdBuffer[0].ElemCount=12;
                ImDrawData Images;
                Images.Valid=true; Images.CmdLists.push_back(&Merged); Images.CmdListsCount=1;
                Images.TotalVtxCount=4; Images.TotalIdxCount=12;
                Images.DisplayPos=Data.DisplayPos; Images.DisplaySize=Data.DisplaySize;
                Images.FramebufferScale=Data.FramebufferScale;
                FImGuiDiagnosticRange Range{&Merged,3,6};
                FUIDrawSnapshot Image(7,10,3,4);
                Check(FImGuiDrawAdapter::Extract(Images,Resolve,150,120,Image,&Range)==ERHIResult::Success &&
                    Image.GetCommands().size()==3 && Image.GetCommands()[0].FirstIndex==0 &&
                    Image.GetCommands()[0].IndexCount==3 && !Image.GetCommands()[0].bDiagnosticWidget &&
                    Image.GetCommands()[1].FirstIndex==3 && Image.GetCommands()[1].IndexCount==6 &&
                    Image.GetCommands()[1].bDiagnosticWidget && Image.GetCommands()[1].BaseVertex==1 &&
                    Image.GetCommands()[2].FirstIndex==9 && Image.GetCommands()[2].IndexCount==3 &&
                    !Image.GetCommands()[2].bDiagnosticWidget && Image.GetTextureLeases().size()==1,
                    "merged image extraction splits only its index range and preserves neighboring geometry");
                const auto RejectRange = [&](FImGuiDiagnosticRange BadRange)
                {
                    FUIDrawSnapshot Bad(7,10,3,4);
                    return FImGuiDrawAdapter::Extract(Images,Resolve,150,120,Bad,&BadRange)!=ERHIResult::Success &&
                        !Bad.IsPublished() && Bad.GetCommands().empty();
                };
                Check(RejectRange({&Merged,1,6}) && RejectRange({&Merged,3,0}) &&
                    RejectRange({&Merged,9,6}) && RejectRange({&List,3,6}) &&
                    RejectRange({&Merged,std::numeric_limits<Core::uint32>::max(),6}),
                    "unmatched, unaligned, empty and overflowing image ranges cannot publish partial packets");
                Merged.CmdBuffer[0].UserCallback=ImDrawCallback_ResetRenderState;
                Check(RejectRange(Range),"diagnostic image range cannot attach to a reset callback");
                Merged.CmdBuffer[0].UserCallback=nullptr;
                Merged.CmdBuffer.resize(4096);
                for (int I=1; I<4096; ++I) Merged.CmdBuffer[I].UserCallback=ImDrawCallback_ResetRenderState;
                Check(RejectRange(Range),"image splitting respects the existing command budget before allocation");
            }
            const auto Rejects = [&]()
            {
                FUIDrawSnapshot Bad(7, 10, 3, 4);
                return FImGuiDrawAdapter::Extract(Data, Resolve, 150, 120, Bad) != ERHIResult::Success && !Bad.IsPublished();
            };
            Cmd.UserCallback = ArbitraryCallback;
            Check(Rejects() && !CallbackCalled, "arbitrary callbacks reject without invoking user code");
            Cmd.UserCallback = ImDrawCallback_ResetRenderState;
            Cmd.TexRef = ImTextureRef{};
            FUIDrawSnapshot Reset(7, 10, 3, 4);
            Check(FImGuiDrawAdapter::Extract(Data, Resolve, 150, 120, Reset) == ERHIResult::Success &&
                Reset.GetCommands()[0].Operation == EUIDrawOperation::ResetState && Reset.GetTextureLeases().empty(),
                "reset-render-state callback becomes an engine value without retaining callback pointers");
            Cmd.UserCallback = nullptr; Cmd.TexRef = ImTextureRef(ImTextureID{77});
            Cmd.IdxOffset = std::numeric_limits<unsigned int>::max();
            Check(Rejects(), "upstream first-index overflow rejects before publishing");
            Cmd.IdxOffset = 3; Cmd.VtxOffset = 4;
            Check(Rejects(), "upstream base-vertex offset cannot escape its copied list");
            Cmd.VtxOffset = 1; Data.TotalIdxCount = 5;
            Check(Rejects(), "aggregate draw-data counts must match their actual lists");
            Data.TotalIdxCount = 6; Cmd.TexRef = ImTextureRef(ImTextureID{999});
            Check(Rejects(), "unknown texture token cannot publish an unleased snapshot");
            Cmd.TexRef = ImTextureRef(ImTextureID{77});
            List.VtxBuffer[0].uv.x = std::numeric_limits<float>::infinity();
            Check(Rejects(), "non-finite upstream vertex data rejects before copying");
            List.VtxBuffer[0].uv.x = 0;
            Request.Operation = EUITextureOperation::Update; Request.LogicalSlot = 0;
            Request.TextureId = Texture.TextureId; Request.ExpectedGeneration = Texture.TextureId.Generation;
            const auto Updated = Registry.Prepare(Request);
            Registry.Poll();
            Check(Updated.Succeeded() && Registry.GetStatistics().Generations == 2 &&
                Snapshot.GetTextureLeases()[0].GetId() == Texture.TextureId,
                "copied snapshot keeps the exact old generation across atlas replacement");
            Snapshot = {}; Registry.Poll();
            Check(Registry.GetStatistics().Generations == 1,
                "discarding an unsubmitted snapshot releases its old texture generation");
        }
        ImGui::DestroyContext();
    }
    (void)Device->Shutdown();
    return Failed;
}
