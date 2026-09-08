#include "FImGuiDrawAdapter.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace Stoner::Application
{
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
namespace
{
template <typename T> bool ValidVector(const ImVector<T>& Values)
{
    return Values.Size >= 0 && Values.Capacity >= Values.Size &&
        (Values.Size == 0 || Values.Data != nullptr);
}
bool Finite(const ImVec2& V) { return std::isfinite(V.x) && std::isfinite(V.y); }
bool Finite(const ImVec4& V)
{ return std::isfinite(V.x) && std::isfinite(V.y) && std::isfinite(V.z) && std::isfinite(V.w); }
}
ERHIResult FImGuiDrawAdapter::Extract(const ImDrawData& Data,
    const FResolveTexture& ResolveTexture, uint32 Width, uint32 Height,
    FUIDrawSnapshot& OutSnapshot)
{
    static_assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4);
    static_assert(sizeof(ImTextureID) == sizeof(uint64));
    if (!Data.Valid || OutSnapshot.IsPublished() || !ValidVector(Data.CmdLists) ||
        Data.CmdListsCount != Data.CmdLists.Size || Data.CmdListsCount > 4096 ||
        Data.TotalVtxCount < 0 || Data.TotalVtxCount > 262144 ||
        Data.TotalIdxCount < 0 || Data.TotalIdxCount > 786432 ||
        !Finite(Data.DisplayPos) || !Finite(Data.DisplaySize) || !Finite(Data.FramebufferScale) ||
        Data.DisplaySize.x <= 0 || Data.DisplaySize.y <= 0 ||
        Data.FramebufferScale.x <= 0 || Data.FramebufferScale.y <= 0 ||
        Width == 0 || Height == 0 || Width > 4096 || Height > 4096 ||
        static_cast<uint64>(Width) * Height > 7864320 ||
        Data.FramebufferScale.x != static_cast<float>(Width) / Data.DisplaySize.x ||
        Data.FramebufferScale.y != static_cast<float>(Height) / Data.DisplaySize.y)
        return ERHIResult::InvalidState;
    uint64 VertexCount = 0, IndexCount = 0, CommandCount = 0;
    // Bounds, callback policy and finite values are checked before allocation.
    for (const auto* List : Data.CmdLists)
    {
        if (!List || !ValidVector(List->VtxBuffer) || !ValidVector(List->IdxBuffer) ||
            !ValidVector(List->CmdBuffer)) return ERHIResult::InvalidState;
        VertexCount += static_cast<uint64>(List->VtxBuffer.Size);
        IndexCount += static_cast<uint64>(List->IdxBuffer.Size);
        CommandCount += static_cast<uint64>(List->CmdBuffer.Size);
        if (VertexCount > 262144 || IndexCount > 786432 || CommandCount > 4096)
            return ERHIResult::Unavailable;
        for (const auto& Vertex : List->VtxBuffer)
            if (!Finite(Vertex.pos) || !Finite(Vertex.uv)) return ERHIResult::InvalidState;
        for (const auto& Command : List->CmdBuffer)
        {
            if (Command.UserCallback)
            {
                if (Command.UserCallback != ImDrawCallback_ResetRenderState) return ERHIResult::Unsupported;
                continue;
            }
            if (!Finite(Command.ClipRect)) return ERHIResult::InvalidState;
            if (Command.ElemCount == 0) continue;
            if (Command.ElemCount % 3 != 0 || Command.IdxOffset > static_cast<uint32>(List->IdxBuffer.Size) ||
                Command.ElemCount > static_cast<uint32>(List->IdxBuffer.Size) - Command.IdxOffset)
                return ERHIResult::InvalidState;
            for (uint32 I = 0; I < Command.ElemCount; ++I)
                if (static_cast<uint64>(List->IdxBuffer[Command.IdxOffset + I]) + Command.VtxOffset >=
                    static_cast<uint64>(List->VtxBuffer.Size)) return ERHIResult::InvalidState;
            if (Command.TexRef._TexData && Command.TexRef._TexData->Status != ImTextureStatus_OK)
                return ERHIResult::NotReady;
        }
    }
    if (VertexCount != static_cast<uint64>(Data.TotalVtxCount) ||
        IndexCount != static_cast<uint64>(Data.TotalIdxCount)) return ERHIResult::InvalidState;
    try
    {
        TArray<FUIVertex> Vertices; TArray<uint32> Indices;
        TArray<FUIDrawCommand> Commands; TArray<FUITextureLease> Leases;
        Vertices.reserve(static_cast<std::size_t>(VertexCount));
        Indices.reserve(static_cast<std::size_t>(IndexCount));
        Commands.reserve(static_cast<std::size_t>(CommandCount));
        Leases.reserve(static_cast<std::size_t>(std::min<uint64>(CommandCount, 512)));
        for (const auto* List : Data.CmdLists)
        {
            const auto VertexBase = static_cast<uint32>(Vertices.size());
            const auto IndexBase = static_cast<uint32>(Indices.size());
            for (const auto& Vertex : List->VtxBuffer)
            {
                const uint32 Color = (((Vertex.col >> IM_COL32_R_SHIFT) & 255u) << FUIVertex::PackedRedShift) |
                    (((Vertex.col >> IM_COL32_G_SHIFT) & 255u) << FUIVertex::PackedGreenShift) |
                    (((Vertex.col >> IM_COL32_B_SHIFT) & 255u) << FUIVertex::PackedBlueShift) |
                    (((Vertex.col >> IM_COL32_A_SHIFT) & 255u) << FUIVertex::PackedAlphaShift);
                Vertices.push_back({{Vertex.pos.x, Vertex.pos.y}, {Vertex.uv.x, Vertex.uv.y}, Color});
            }
            for (const auto Index : List->IdxBuffer) Indices.push_back(static_cast<uint32>(Index));
            for (const auto& Source : List->CmdBuffer)
            {
                FUIDrawCommand Command;
                if (Source.UserCallback == ImDrawCallback_ResetRenderState)
                    Command.Operation = EUIDrawOperation::ResetState;
                else
                {
                    if (Source.ElemCount == 0) continue;
                    if (!ResolveTexture) return ERHIResult::InvalidState;
                    auto Lease = ResolveTexture(static_cast<uint64>(Source.GetTexID()));
                    if (!Lease.IsValid()) return ERHIResult::NotReady;
                    Command.TextureId = Lease.GetId();
                    if (std::none_of(Leases.begin(), Leases.end(),
                        [&](const auto& Existing) { return Existing.GetId() == Lease.GetId(); }))
                    {
                        if (Leases.size() == 512) return ERHIResult::Unavailable;
                        Leases.push_back(std::move(Lease));
                    }
                    Command.FirstIndex = IndexBase + Source.IdxOffset;
                    Command.IndexCount = Source.ElemCount;
                    Command.BaseVertex = static_cast<int32>(VertexBase + Source.VtxOffset);
                    Command.ClipRect = {Source.ClipRect.x, Source.ClipRect.y, Source.ClipRect.z, Source.ClipRect.w};
                }
                Commands.push_back(Command);
            }
        }
        FUIDrawSnapshot Candidate(OutSnapshot.GetSessionId(), OutSnapshot.GetFrameId(),
            OutSnapshot.GetSettingsRevision(), OutSnapshot.GetDisplayGeneration());
        if (!Candidate.SetDisplay({Data.DisplayPos.x, Data.DisplayPos.y}, {Data.DisplaySize.x, Data.DisplaySize.y},
                {Data.FramebufferScale.x, Data.FramebufferScale.y}) ||
            !Candidate.SetVertices(Vertices) || !Candidate.SetIndices(Indices) ||
            !Candidate.SetCommands(Commands) || !Candidate.SetTextureLeases(Leases) ||
            !Candidate.Publish() || !Candidate.ValidateOwnedGeometry(Width, Height)) return ERHIResult::InvalidState;
        OutSnapshot = std::move(Candidate);
        return ERHIResult::Success;
    }
    catch (const std::bad_alloc&) { return ERHIResult::Unavailable; }
}
}
