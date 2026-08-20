#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Backend::Metal
{

struct FMetalBackendInspection
{
    Core::uint64 OwnerIdentity = 0;
    Core::uint64 Generation = 0;
    Core::uint64 DeviceOwnershipCount = 0;
    Core::uint64 LiveObjectCount = 0;
    Core::uint64 ResourceOwnershipCount = 0;
    Core::uint64 PipelineOwnershipCount = 0;
    Core::uint64 CommandOwnershipCount = 0;
    Core::uint64 SynchronizationOwnershipCount = 0;
    Core::uint64 SubmissionOwnershipCount = 0;
    Core::uint64 PresentationOwnershipCount = 0;
    Core::uint64 InFlightSubmissionCount = 0;
    bool bAcceptingWork = false;
    bool bTerminalFailure = false;
    Core::FString TerminalFailureReason;
};

} // namespace Stoner::Backend::Metal
