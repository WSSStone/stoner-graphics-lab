#pragma once
#include "FLabPresetCodec.h"
#include "Core/FPlatformFileSystem.h"
#include <functional>

namespace Stoner::Application
{
struct FLabPresetStoreConfig
{
    // Existing canonical directory, owned by the application/user for export.
    Stoner::Core::FString ExportRoot;
    // Existing canonical content/cooked/baseline/formal roots and read-only
    // imported files. The composition root supplies the full protection set.
    Stoner::Core::TArray<Stoner::Core::FString> ProtectedPaths;
};
struct FLabPresetExportResult
{
    Stoner::Core::FPlatformFileStatus Status;
    Stoner::Core::FString TargetPath;
    bool bPublished=false;
    bool bTemporaryRetained=false;
};
class FLabPresetStore
{
public:
    [[nodiscard]] static Stoner::Core::FPlatformFileStatus Read(
        const Stoner::Core::FString& Path,const FLabPresetWorkload& Expected,
        std::span<const FLabDebugStage> Stages,FLabPreset& OutPreset);
    [[nodiscard]] static FLabPresetExportResult Export(const FLabPresetStoreConfig& Config,
        const Stoner::Core::FString& Filename,const FLabPreset& Preset,
        std::span<const FLabDebugStage> Stages,bool bOverwrite=false,
        // Private test seam at the durable-temp/atomic-publish boundary.
        const std::function<bool(const Stoner::Core::FString&,const Stoner::Core::FString&)>& BeforePublish={});
};
}
