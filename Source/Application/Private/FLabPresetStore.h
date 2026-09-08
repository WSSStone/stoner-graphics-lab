#pragma once
#include "FLabPresetCodec.h"
#include "Application/FLabPresetStorage.h"
#include <functional>

namespace Stoner::Application
{
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
