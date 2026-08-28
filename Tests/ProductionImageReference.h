#pragma once

#include "ProductionImageAcceptance.h"

#include <filesystem>

[[nodiscard]] bool LoadProductionReferenceImage(
    const std::filesystem::path& Path,
    EProductionColorTransfer Transfer,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure);
