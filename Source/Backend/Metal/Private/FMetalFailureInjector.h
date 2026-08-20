#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Backend::Metal::Private
{

enum class EMetalFailurePoint : Core::uint8
{
    DeviceInitialization,
    ResourceAllocation,
    PipelineCreation,
    CommandRecording,
    CommandSubmission,
    Synchronization,
    DrawableAcquisition,
    Presentation,
    Shutdown
};

[[nodiscard]] const char* ToStableName(EMetalFailurePoint Point) noexcept;

class FMetalFailureInjector
{
public:
    static void ConfigureOneShot(EMetalFailurePoint Point);
    static void ConfigureSequence(
        const Core::TArray<EMetalFailurePoint>& Points);
    static void Reset() noexcept;
    [[nodiscard]] static bool ShouldFail(EMetalFailurePoint Point) noexcept;
    [[nodiscard]] static Core::uint64 GetConsumedCount() noexcept;
    [[nodiscard]] static Core::uint64 GetRemainingCount() noexcept;
};

class FScopedMetalFailureInjection
{
public:
    explicit FScopedMetalFailureInjection(EMetalFailurePoint Point);
    explicit FScopedMetalFailureInjection(
        const Core::TArray<EMetalFailurePoint>& Points);
    ~FScopedMetalFailureInjection();

    FScopedMetalFailureInjection(const FScopedMetalFailureInjection&) = delete;
    FScopedMetalFailureInjection& operator=(
        const FScopedMetalFailureInjection&) = delete;
};

} // namespace Stoner::Backend::Metal::Private
