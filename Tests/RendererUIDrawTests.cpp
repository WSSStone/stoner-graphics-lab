// T036: packet failure fixtures precede the private validator implementation.
// Native texture lifecycle fixtures join this suite with the registry.
#include "FUIDrawValidator.h"
#include "RHI/FRHIBufferTextureCopyRegion.h"
#include <iostream>
#include <limits>

namespace
{
using namespace Stoner::Renderer;
using namespace Stoner::Core;
int Passed = 0, Failed = 0;
void Check(bool OK, const char* Name)
{
    (OK ? ++Passed : ++Failed);
    std::cout << (OK ? "[PASS] " : "[FAIL] ") << Name << '\n';
}
FUIDrawValidationContext Context()
{
    return {7, 3, 4, 8, 150, 120, [](FUITextureId ID) { return ID == FUITextureId{1, 2}; }};
}
FUIDrawSnapshot Packet(FUIDrawCommand Command = {3, 3, 1, {8.5f, 18.5f, 30.1f, 40.1f}, {1, 2}},
    TArray<uint32> Indices = {999, 999, 999, 0, 1, 2}, bool bPublish = true)
{
    FUIDrawSnapshot Result(7, 9, 3, 4);
    TArray<FUIVertex> Vertices(4);
    Vertices[1].Position = {10, 20};
    Vertices[2].Position = {30, 20};
    Vertices[3].Position = {10, 40};
    (void)Result.SetDisplay({10, 20}, {100, 80}, {1.5f, 1.5f});
    (void)Result.SetVertices(Vertices);
    (void)Result.SetIndices(Indices);
    (void)Result.SetCommands(TArray<FUIDrawCommand>{Command});
    (void)Result.SetTextureIds(TArray<FUITextureId>{{1, 2}});
    if (bPublish) (void)Result.Publish();
    return Result;
}
}

int RunRendererUIDrawTests()
{
    Passed = Failed = 0;
    Stoner::RHI::FRHIBufferTextureCopyRegion Upload;
    Upload.Width = 4; Upload.Height = 3; Upload.SourceRowLengthTexels = 64;
    uint64 Required = 0;
    Check(Stoner::RHI::TryGetRHIBufferTextureCopyByteSize(Upload,
        Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm, Required) && Required == 528,
        "upload footprint includes aligned row gaps and final tight row");
    Upload.SourceRowLengthTexels = 3;
    Check(!Stoner::RHI::TryGetRHIBufferTextureCopyByteSize(Upload,
        Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm, Required), "upload rejects an undersized source pitch");
    auto C = Context();
    auto Good = Packet();
    auto V = FUIDrawValidator::Validate(Good, C);
    Check(V.bValid && V.Commands.size() == 1 && V.Commands[0].Draw.FirstIndex == 3 &&
        V.Commands[0].Draw.BaseVertex == 1, "UI packet preserves nonzero index and base-vertex offsets");
    Check(V.bValid && V.Commands[0].ScissorX == 0 && V.Commands[0].ScissorY == 0 &&
        V.Commands[0].ScissorWidth == 31 && V.Commands[0].ScissorHeight == 31,
        "logical clip origin and fractional scale clamp before unsigned conversion");
    auto Command = Good.GetCommands()[0];
    Command.BaseVertex = -1;
    Check(!FUIDrawValidator::Validate(Packet(Command), C).bValid,
        "negative effective vertex rejects the packet");
    Command.BaseVertex = 0;
    Check(FUIDrawValidator::Validate(Packet(Command), C).bValid,
        "zero base vertex remains valid");
    Command.BaseVertex = 1;
    Check(!FUIDrawValidator::Validate(Packet(Command, {0, 0, 0, 0, 1, 3}), C).bValid,
        "effective vertex past the copied vertex range rejects");
    Command.FirstIndex = std::numeric_limits<uint32>::max();
    Check(!FUIDrawValidator::Validate(Packet(Command), C).bValid,
        "first-index arithmetic cannot wrap into a valid range");
    Command = Good.GetCommands()[0]; Command.IndexCount = 2;
    Check(!FUIDrawValidator::Validate(Packet(Command), C).bValid,
        "incomplete triangles reject");
    Command.IndexCount = 6;
    Check(!FUIDrawValidator::Validate(Packet(Command), C).bValid,
        "index count past remaining copied indices rejects");
    Command = Good.GetCommands()[0]; Command.ClipRect = {-100, -100, 0, 0};
    V = FUIDrawValidator::Validate(Packet(Command), C);
    Check(V.bValid && V.Commands.empty(), "fully clipped UI commands require no native work");
    Command.IndexCount = 0; Command.FirstIndex = std::numeric_limits<uint32>::max();
    Check(FUIDrawValidator::Validate(Packet(Command), C).bValid, "empty draw is a safe no-op");
    Command = {}; Command.Operation = EUIDrawOperation::ResetState;
    V = FUIDrawValidator::Validate(Packet(Command), C);
    Check(V.bValid && V.Commands.size() == 1 && V.Commands[0].Draw.Operation == EUIDrawOperation::ResetState,
        "typed reset-state operation survives preparation");
    Command.Operation = static_cast<EUIDrawOperation>(999);
    Check(!FUIDrawValidator::Validate(Packet(Command), C).bValid,
        "unknown callback operation cannot publish or record");
    C.HasTextureLease = {};
    Check(!FUIDrawValidator::Validate(Good, C).bValid, "missing texture lease resolver rejects a sampled command");
    C = Context(); C.HasTextureLease = [](FUITextureId) { return false; };
    Check(!FUIDrawValidator::Validate(Good, C).bValid, "unleased texture generation rejects");
    C = Context(); C.DisplayGeneration = 5;
    Check(!FUIDrawValidator::Validate(Good, C).bValid, "stale display generation rejects");
    C = Context(); C.SettingsRevision = 4;
    Check(!FUIDrawValidator::Validate(Good, C).bValid, "stale settings revision rejects");
    C = Context(); C.SessionId = 8;
    Check(!FUIDrawValidator::Validate(Good, C).bValid, "cross-session packet rejects");
    C = Context(); C.LastSubmittedFrameId = 9;
    Check(!FUIDrawValidator::Validate(Good, C).bValid, "already-submitted frame cannot be replayed");
    C = Context(); C.DrawableWidth = 151;
    Check(!FUIDrawValidator::Validate(Good, C).bValid, "mismatched drawable scale rejects");
    C = Context();
    auto Unpublished = Packet(Good.GetCommands()[0], {999, 999, 999, 0, 1, 2}, false);
    Check(!FUIDrawValidator::Validate(Unpublished, C).bValid, "unpublished mutable packet rejects");
    TArray<FUIVertex> Vertices(4);
    (void)Unpublished.SetVertices(Vertices);
    Vertices[0].Position.X = std::numeric_limits<float>::quiet_NaN();
    Check(Unpublished.GetVertices()[0].Position.X == 0,
        "packet owns copied vertex bytes independently of source mutation");
    (void)Unpublished.SetVertices(Vertices);
    Check(!Unpublished.Publish(), "nonfinite copied vertex cannot publish");
    auto MissingId = Packet(Good.GetCommands()[0], {999, 999, 999, 0, 1, 2}, false);
    (void)MissingId.SetTextureIds({}); (void)MissingId.Publish();
    Check(!FUIDrawValidator::Validate(MissingId, C).bValid,
        "sampled texture must appear in the packet's declared generations");
    auto Duplicate = Packet(Good.GetCommands()[0], {999, 999, 999, 0, 1, 2}, false);
    (void)Duplicate.SetTextureIds(TArray<FUITextureId>{{1, 2}, {1, 2}}); (void)Duplicate.Publish();
    Check(!FUIDrawValidator::Validate(Duplicate, C).bValid,
        "duplicate generation declarations reject");
    Command = Good.GetCommands()[0];
    Command.ClipRect = {-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    V = FUIDrawValidator::Validate(Packet(Command), C);
    Check(V.bValid && V.Commands[0].ScissorWidth == 150 && V.Commands[0].ScissorHeight == 120,
        "finite extreme clip coordinates clamp without float intermediate overflow");
    std::cout << "Renderer UI draw: " << Passed << " passed, " << Failed << " failed\n";
    return Failed;
}
