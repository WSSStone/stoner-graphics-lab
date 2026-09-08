#pragma once
#include "Application/FLabPreset.h"
#include "Core/FPlatformFileSystem.h"

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
}
