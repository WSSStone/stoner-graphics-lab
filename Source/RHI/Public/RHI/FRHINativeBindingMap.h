#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIDescriptorType.h"
#include "RHI/ERHIShaderStage.h"

#include <array>
#include <span>

namespace Stoner::RHI
{

struct FRHISha256Digest
{
    bool bAvailable = false;
    std::array<Stoner::Core::uint8, 32> Bytes{};

    [[nodiscard]] bool operator==(const FRHISha256Digest&) const = default;
};

enum class ERHINativeResourceClass : Stoner::Core::uint8
{
    Buffer,
    Texture,
    Sampler
};

struct FRHINativeBindingEntry
{
    ERHIShaderStage Stage = ERHIShaderStage::Unknown;
    Stoner::Core::uint32 SetIndex = 0;
    Stoner::Core::uint32 BindingSlot = 0;
    ERHIDescriptorType DescriptorType = ERHIDescriptorType::UniformBuffer;
    Stoner::Core::uint32 ArrayElement = 0;
    ERHINativeResourceClass NativeClass = ERHINativeResourceClass::Buffer;
    Stoner::Core::uint32 NativeIndex = 0;

    [[nodiscard]] bool operator==(const FRHINativeBindingEntry&) const = default;
};

struct FRHINativeBindingReservedRange
{
    ERHIShaderStage Stage = ERHIShaderStage::Unknown;
    ERHINativeResourceClass NativeClass = ERHINativeResourceClass::Buffer;
    Stoner::Core::uint32 FirstIndex = 0;
    Stoner::Core::uint32 Count = 0;
    Stoner::Core::FString Purpose;

    [[nodiscard]] bool operator==(
        const FRHINativeBindingReservedRange&) const = default;
};

struct FRHINativeBindingLimit
{
    ERHIShaderStage Stage = ERHIShaderStage::Unknown;
    ERHINativeResourceClass NativeClass = ERHINativeResourceClass::Buffer;
    Stoner::Core::uint32 MaxCount = 0;

    [[nodiscard]] bool operator==(const FRHINativeBindingLimit&) const = default;
};

struct FRHINativeBindingMap
{
    Stoner::Core::FString PolicyVersion;
    Stoner::Core::TArray<FRHINativeBindingEntry> Entries;
    Stoner::Core::TArray<FRHINativeBindingReservedRange> ReservedRanges;
    Stoner::Core::TArray<FRHINativeBindingLimit> LimitSnapshot;
    FRHISha256Digest CanonicalDigest;
};

[[nodiscard]] FRHISha256Digest ComputeRHISha256(
    std::span<const Stoner::Core::uint8> Bytes);
[[nodiscard]] bool TryComputeRHISha256(
    std::span<const Stoner::Core::uint8> Bytes,
    FRHISha256Digest& OutDigest) noexcept;
[[nodiscard]] bool ComputeRHINativeBindingMapDigest(
    const FRHINativeBindingMap& Map,
    FRHISha256Digest& OutDigest) noexcept;
[[nodiscard]] bool FinalizeRHINativeBindingMapDigest(
    FRHINativeBindingMap& Map) noexcept;
[[nodiscard]] bool IsCanonicalRHINativeBindingMap(
    const FRHINativeBindingMap& Map) noexcept;

} // namespace Stoner::RHI
