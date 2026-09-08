#pragma once
#include "Application/FLabPreset.h"
#include <span>

namespace Stoner::Application
{
class FLabPresetCodec
{
public:
    static constexpr Stoner::Core::usize MaximumBytes=65536;
    [[nodiscard]] static bool Decode(std::span<const Stoner::Core::uint8> Bytes,
        const FLabPresetWorkload& ExpectedWorkload, std::span<const FLabDebugStage> Stages,
        FLabPreset& OutPreset, Stoner::Core::FString& OutReason);
    [[nodiscard]] static bool Encode(const FLabPreset& Preset,
        std::span<const FLabDebugStage> Stages,
        Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes,
        Stoner::Core::FString& OutReason);
};
}
