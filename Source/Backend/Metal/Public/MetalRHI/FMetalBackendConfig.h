#pragma once

#include "Core/CoreMinimal.h"

#include <optional>

namespace Stoner::Backend::Metal
{

enum class EMetalAdapterPreference : Core::uint8
{
    Canonical,
    LowPower,
    HighPerformance
};

enum class EMetalInitializationFailurePoint : Core::uint8
{
    None,
    AfterAdapterSelection,
    BeforeQueueCreation
};

struct FMetalBackendConfig
{
    std::optional<Core::uint64> PreferredRegistryId;
    EMetalAdapterPreference AdapterPreference =
        EMetalAdapterPreference::Canonical;
    bool bRequirePresentation = false;
    bool bEnableValidation = false;
    EMetalInitializationFailurePoint FailurePoint =
        EMetalInitializationFailurePoint::None;
};

} // namespace Stoner::Backend::Metal
