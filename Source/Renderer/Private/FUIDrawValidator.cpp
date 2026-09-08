#include "FUIDrawValidator.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Stoner::Renderer
{
FUIDrawValidationResult FUIDrawValidator::Validate(
    const FUIDrawSnapshot& Snapshot, const FUIDrawValidationContext& Context)
{
    auto Reject = [](const char* Diagnostic) {
        return FUIDrawValidationResult{false, Diagnostic, {}};
    };
    if (!Snapshot.IsPublished() || !Snapshot.IsValid()) return Reject("ui-packet-not-published-or-invalid");
    if (Snapshot.GetSessionId() != Context.SessionId ||
        Snapshot.GetSettingsRevision() != Context.SettingsRevision ||
        Snapshot.GetDisplayGeneration() != Context.DisplayGeneration ||
        Snapshot.GetFrameId() <= Context.LastSubmittedFrameId)
        return Reject("ui-packet-stale-identity");
    const auto Width = Context.DrawableWidth;
    const auto Height = Context.DrawableHeight;
    if (!Width || !Height || Width > 4096 || Height > 4096 ||
        static_cast<Stoner::Core::uint64>(Width) * Height > 7864320)
        return Reject("ui-packet-drawable-budget");
    const auto Size = Snapshot.GetDisplaySize();
    const auto Scale = Snapshot.GetFramebufferScale();
    if (Scale.X != static_cast<float>(Width) / Size.X ||
        Scale.Y != static_cast<float>(Height) / Size.Y)
        return Reject("ui-packet-drawable-scale-mismatch");
    const auto& Vertices = Snapshot.GetVertices();
    const auto& Indices = Snapshot.GetIndices();
    const auto& Commands = Snapshot.GetCommands();
    const auto& TextureIds = Snapshot.GetTextureIds();
    Stoner::Core::uint64 Bytes = sizeof(FUIDrawSnapshot);
    auto AddBytes = [&](std::size_t Count, std::size_t Stride) {
        constexpr auto Limit = 9ull * 1024 * 1024;
        if (Stride == 0 || Count > (Limit - Bytes) / Stride) return false;
        Bytes += Count * Stride;
        return true;
    };
    if (!AddBytes(Vertices.size(), sizeof(FUIVertex)) ||
        !AddBytes(Indices.size(), sizeof(Stoner::Core::uint32)) ||
        !AddBytes(Commands.size(), sizeof(FUIDrawCommand)) ||
        !AddBytes(TextureIds.size(), sizeof(FUITextureId)) ||
        !AddBytes(Snapshot.GetTextureLeases().size(), sizeof(FUITextureLease)))
        return Reject("ui-packet-staging-budget");
    for (std::size_t I = 0; I < TextureIds.size(); ++I)
    {
        if (!Context.HasTextureLease || !Context.HasTextureLease(TextureIds[I]))
            return Reject("ui-packet-unleased-texture-generation");
        if (std::find(TextureIds.begin(), TextureIds.begin() + I, TextureIds[I]) != TextureIds.begin() + I)
            return Reject("ui-packet-duplicate-texture-generation");
    }

    FUIDrawValidationResult Result;
    Result.Commands.reserve(Commands.size());
    const auto Origin = Snapshot.GetDisplayPos();
    for (const auto& Command : Commands)
    {
        if (Command.bDiagnosticWidget && !Context.bAllowDiagnosticRequests)
            return Reject("ui-packet-unresolved-diagnostic-widget");
        if (Command.Operation == EUIDrawOperation::ResetState)
        {
            Result.Commands.push_back({Command, 0, 0, 0, 0});
            continue;
        }
        if (Command.Operation != EUIDrawOperation::Draw) return Reject("ui-packet-unknown-operation");
        if (Command.IndexCount == 0) continue;
        if (Command.IndexCount % 3 != 0 || Command.FirstIndex > Indices.size() ||
            Command.IndexCount > Indices.size() - Command.FirstIndex)
            return Reject("ui-packet-index-range");
        const auto End = static_cast<Stoner::Core::uint64>(Command.FirstIndex) + Command.IndexCount;
        for (Stoner::Core::uint64 I = Command.FirstIndex; I < End; ++I)
        {
            const auto Vertex = static_cast<Stoner::Core::int64>(Indices[I]) + Command.BaseVertex;
            if (Vertex < 0 || static_cast<Stoner::Core::uint64>(Vertex) >= Vertices.size())
                return Reject("ui-packet-effective-vertex-range");
        }
        // Promote before subtraction: opposite finite float extrema must not
        // overflow the intermediate coordinate. Clamp while still signed.
        const double X0 = std::clamp(std::floor((static_cast<double>(Command.ClipRect.X) - Origin.X) * Scale.X), 0.0, static_cast<double>(Width));
        const double Y0 = std::clamp(std::floor((static_cast<double>(Command.ClipRect.Y) - Origin.Y) * Scale.Y), 0.0, static_cast<double>(Height));
        const double X1 = std::clamp(std::ceil((static_cast<double>(Command.ClipRect.Z) - Origin.X) * Scale.X), 0.0, static_cast<double>(Width));
        const double Y1 = std::clamp(std::ceil((static_cast<double>(Command.ClipRect.W) - Origin.Y) * Scale.Y), 0.0, static_cast<double>(Height));
        if (X1 <= X0 || Y1 <= Y0) continue;
        if (std::find(TextureIds.begin(), TextureIds.end(), Command.TextureId) == TextureIds.end() ||
            !Context.HasTextureLease || !Context.HasTextureLease(Command.TextureId))
            return Reject("ui-packet-unleased-texture-generation");
        Result.Commands.push_back({Command,
            static_cast<Stoner::Core::uint32>(X0), static_cast<Stoner::Core::uint32>(Y0),
            static_cast<Stoner::Core::uint32>(X1 - X0), static_cast<Stoner::Core::uint32>(Y1 - Y0)});
    }
    Result.bValid = true;
    return Result;
}
}
