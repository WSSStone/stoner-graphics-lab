#include "FInteractiveLabShaders.h"
#include "Core/FPlatformFileSystem.h"
#include <cstdlib>
#include <iostream>
#include <filesystem>

int RunInteractiveLabShaderTests()
{
    using namespace Stoner;
    using namespace Stoner::Demo;
    int Failed = 0;
    const auto Check = [&](bool OK, const char* Name)
    { std::cout << (OK ? "[PASS] " : "[FAIL] ") << Name << '\n'; if (!OK) ++Failed; };
    FInteractiveLabShaders Shaders;
    Core::FString Reason;
    Check(PrepareInteractiveLabShaders({}, {}, {}, Shaders, Reason) != Asset::EAssetResult::Success &&
        !Shaders.Generation.IsAvailable() && !Reason.IsEmpty(), "UI shader preflight requires an authenticated generation identity");
    const char* Root = std::getenv("STONER_LAB_SHADER_TEST_PUBLICATION");
    const char* Profile = std::getenv("STONER_LAB_SHADER_TEST_PROFILE");
    if (!Root || !Profile)
    {
        std::cout << "[SKIP] cooked UI shader selection requires STONER_LAB_SHADER_TEST_PUBLICATION and PROFILE\n";
        return Failed;
    }
    Core::TArray<Core::uint8> Bytes;
    Asset::FCurrentGenerationPointer Pointer;
    Asset::FAssetTargetProfileEvidence Target;
    const bool Parsed = Core::FPlatformFileSystem::ReadFile(
        (std::filesystem::path(Root) / "Current.json").string(), Bytes) &&
        Asset::FAssetCookContractCodec::ParseCurrentPointer(Bytes, Pointer) == Asset::EAssetResult::Success &&
        Core::FPlatformFileSystem::ReadFile(Profile, Bytes) &&
        Asset::FAssetCookContractCodec::ParseTargetProfile(Bytes, Target) == Asset::EAssetResult::Success;
    Check(Parsed, "cooked UI shader fixture parses target and current generation");
    if (!Parsed) return Failed;
    FProductionContentSession Session;
    FProductionContentSessionConfig Config;
    Config.PublicationRoot = Root;
    Config.LeaseCoordinationRoot = (std::filesystem::path(Root).parent_path() / "shader-test-leases").string();
    std::filesystem::create_directories(Config.LeaseCoordinationRoot.ToStdString());
    Config.RootAssetIdentity = "StaticModel:Lantern.glb#idx.scene.0";
    Config.ExpectedGeneration = Pointer.GenerationId;
    Config.TargetEvidence = Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(Target);
    FProductionContentLoadedClosure Closure;
    const bool Loaded = Session.Load(Config, Closure) == Asset::EAssetResult::Success;
    Check(Loaded, "strict cooked lab closure loads without source fallback");
    if (!Loaded) { std::cout << Session.Inspect().FirstFailure.CStr() << '\n'; return Failed; }
    const bool Prepared = PrepareInteractiveLabShaders(Closure, Pointer.GenerationId, Target, Shaders, Reason) ==
        Asset::EAssetResult::Success;
    Check(Prepared && Shaders.Draw.ModuleDescriptions.size() == 2 && Shaders.Copy.ModuleDescriptions.size() == 2 &&
        Shaders.Generation == Closure.GenerationIdentity, "UI draw and copy bytecode select from the same cooked generation and target");
    if (Prepared)
    {
        auto Bad = Closure;
        Bad.RenderShaders.clear();
        Check(PrepareInteractiveLabShaders(Bad, Pointer.GenerationId, Target, Shaders, Reason) != Asset::EAssetResult::Success &&
            Shaders.Draw.ModuleDescriptions.size() == 2, "scene-only closure rejects UI enable without replacing prepared shaders");
        Bad = Closure; Bad.RenderShaderPayloads.clear();
        Check(PrepareInteractiveLabShaders(Bad, Pointer.GenerationId, Target, Shaders, Reason) != Asset::EAssetResult::Success,
            "missing cooked UI payloads cannot fall back to source bytes");
        Bad = Closure; Bad.RenderShaders.insert(Bad.RenderShaders.end(), Closure.RenderShaders.begin(), Closure.RenderShaders.end());
        Check(PrepareInteractiveLabShaders(Bad, Pointer.GenerationId, Target, Shaders, Reason) != Asset::EAssetResult::Success,
            "ambiguous duplicate UI programs reject preflight");
        Bad = Closure; Bad.GenerationIdentity = {};
        Check(PrepareInteractiveLabShaders(Bad, Pointer.GenerationId, Target, Shaders, Reason) != Asset::EAssetResult::Success,
            "UI enable cannot select another generation");
        auto OtherTarget = Target;
        OtherTarget.Profile.GraphicsBackend = Target.Profile.GraphicsBackend == Asset::EAssetGraphicsBackend::Metal
            ? Asset::EAssetGraphicsBackend::Vulkan : Asset::EAssetGraphicsBackend::Metal;
        Check(PrepareInteractiveLabShaders(Closure, Pointer.GenerationId, OtherTarget, Shaders, Reason) != Asset::EAssetResult::Success,
            "UI shader preflight rejects a mismatched backend target");
    }
    Check(Session.Shutdown() == Asset::EAssetResult::Success, "cooked shader fixture shuts down cleanly");
    return Failed;
}
