#include "RHI/FRHINativeBindingMap.h"

#include <algorithm>
#include <array>
#include <limits>
#include <tuple>
#include <vector>

namespace Stoner::RHI
{
namespace
{

constexpr std::array<Stoner::Core::uint32, 64> Sha256Constants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

[[nodiscard]] Stoner::Core::uint32 RotateRight(
    Stoner::Core::uint32 Value,
    Stoner::Core::uint32 Count) noexcept
{
    return (Value >> Count) | (Value << (32U - Count));
}

void AppendU32(
    Stoner::Core::TArray<Stoner::Core::uint8>& Bytes,
    Stoner::Core::uint32 Value)
{
    Bytes.push_back(static_cast<Stoner::Core::uint8>(Value));
    Bytes.push_back(static_cast<Stoner::Core::uint8>(Value >> 8U));
    Bytes.push_back(static_cast<Stoner::Core::uint8>(Value >> 16U));
    Bytes.push_back(static_cast<Stoner::Core::uint8>(Value >> 24U));
}

void AppendText(
    Stoner::Core::TArray<Stoner::Core::uint8>& Bytes,
    const Stoner::Core::FString& Text)
{
    AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Text.Len()));
    Bytes.insert(Bytes.end(), Text.View().begin(), Text.View().end());
}

[[nodiscard]] auto SourceKey(const FRHINativeBindingEntry& Entry) noexcept
{
    return std::tuple(
        Entry.Stage,
        Entry.SetIndex,
        Entry.BindingSlot,
        Entry.DescriptorType,
        Entry.ArrayElement,
        Entry.NativeClass);
}

[[nodiscard]] auto NativeKey(const FRHINativeBindingEntry& Entry) noexcept
{
    return std::tuple(Entry.Stage, Entry.NativeClass, Entry.NativeIndex);
}

[[nodiscard]] auto RangeKey(const FRHINativeBindingReservedRange& Range) noexcept
{
    return std::tuple(Range.Stage, Range.NativeClass, Range.FirstIndex);
}

[[nodiscard]] auto LimitKey(const FRHINativeBindingLimit& Limit) noexcept
{
    return std::tuple(Limit.Stage, Limit.NativeClass);
}

[[nodiscard]] bool IsSupportedStage(ERHIShaderStage Stage) noexcept
{
    return Stage == ERHIShaderStage::Vertex ||
        Stage == ERHIShaderStage::Fragment ||
        Stage == ERHIShaderStage::Compute;
}

[[nodiscard]] bool IsNativeClass(
    ERHINativeResourceClass NativeClass) noexcept
{
    return NativeClass == ERHINativeResourceClass::Buffer ||
        NativeClass == ERHINativeResourceClass::Texture ||
        NativeClass == ERHINativeResourceClass::Sampler;
}

[[nodiscard]] bool MatchesDescriptorClass(
    ERHIDescriptorType Type,
    ERHINativeResourceClass NativeClass) noexcept
{
    switch (Type)
    {
    case ERHIDescriptorType::UniformBuffer:
    case ERHIDescriptorType::StorageBuffer:
        return NativeClass == ERHINativeResourceClass::Buffer;
    case ERHIDescriptorType::SampledTexture:
    case ERHIDescriptorType::StorageTexture:
        return NativeClass == ERHINativeResourceClass::Texture;
    case ERHIDescriptorType::Sampler:
        return NativeClass == ERHINativeResourceClass::Sampler;
    case ERHIDescriptorType::CombinedTextureSampler:
        return NativeClass == ERHINativeResourceClass::Texture ||
            NativeClass == ERHINativeResourceClass::Sampler;
    }
    return false;
}

} // namespace

FRHISha256Digest ComputeRHISha256(
    std::span<const Stoner::Core::uint8> Bytes)
{
    std::vector<Stoner::Core::uint8> Message(Bytes.begin(), Bytes.end());
    const Stoner::Core::uint64 BitLength =
        static_cast<Stoner::Core::uint64>(Message.size()) * 8U;
    Message.push_back(0x80U);
    while ((Message.size() % 64U) != 56U)
    {
        Message.push_back(0U);
    }
    for (int Shift = 56; Shift >= 0; Shift -= 8)
    {
        Message.push_back(static_cast<Stoner::Core::uint8>(BitLength >> Shift));
    }

    std::array<Stoner::Core::uint32, 8> State = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    for (std::size_t Offset = 0; Offset < Message.size(); Offset += 64U)
    {
        std::array<Stoner::Core::uint32, 64> Words{};
        for (std::size_t Index = 0; Index < 16U; ++Index)
        {
            const std::size_t Base = Offset + Index * 4U;
            Words[Index] =
                (static_cast<Stoner::Core::uint32>(Message[Base]) << 24U) |
                (static_cast<Stoner::Core::uint32>(Message[Base + 1U]) << 16U) |
                (static_cast<Stoner::Core::uint32>(Message[Base + 2U]) << 8U) |
                static_cast<Stoner::Core::uint32>(Message[Base + 3U]);
        }
        for (std::size_t Index = 16U; Index < 64U; ++Index)
        {
            const Stoner::Core::uint32 S0 =
                RotateRight(Words[Index - 15U], 7U) ^
                RotateRight(Words[Index - 15U], 18U) ^
                (Words[Index - 15U] >> 3U);
            const Stoner::Core::uint32 S1 =
                RotateRight(Words[Index - 2U], 17U) ^
                RotateRight(Words[Index - 2U], 19U) ^
                (Words[Index - 2U] >> 10U);
            Words[Index] = Words[Index - 16U] + S0 + Words[Index - 7U] + S1;
        }

        Stoner::Core::uint32 A = State[0];
        Stoner::Core::uint32 B = State[1];
        Stoner::Core::uint32 C = State[2];
        Stoner::Core::uint32 D = State[3];
        Stoner::Core::uint32 E = State[4];
        Stoner::Core::uint32 F = State[5];
        Stoner::Core::uint32 G = State[6];
        Stoner::Core::uint32 H = State[7];
        for (std::size_t Index = 0; Index < 64U; ++Index)
        {
            const Stoner::Core::uint32 S1 =
                RotateRight(E, 6U) ^ RotateRight(E, 11U) ^ RotateRight(E, 25U);
            const Stoner::Core::uint32 Choice = (E & F) ^ ((~E) & G);
            const Stoner::Core::uint32 Temp1 =
                H + S1 + Choice + Sha256Constants[Index] + Words[Index];
            const Stoner::Core::uint32 S0 =
                RotateRight(A, 2U) ^ RotateRight(A, 13U) ^ RotateRight(A, 22U);
            const Stoner::Core::uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const Stoner::Core::uint32 Temp2 = S0 + Majority;
            H = G;
            G = F;
            F = E;
            E = D + Temp1;
            D = C;
            C = B;
            B = A;
            A = Temp1 + Temp2;
        }
        State[0] += A;
        State[1] += B;
        State[2] += C;
        State[3] += D;
        State[4] += E;
        State[5] += F;
        State[6] += G;
        State[7] += H;
    }

    FRHISha256Digest Digest;
    Digest.bAvailable = true;
    for (std::size_t Index = 0; Index < State.size(); ++Index)
    {
        Digest.Bytes[Index * 4U] = static_cast<Stoner::Core::uint8>(State[Index] >> 24U);
        Digest.Bytes[Index * 4U + 1U] = static_cast<Stoner::Core::uint8>(State[Index] >> 16U);
        Digest.Bytes[Index * 4U + 2U] = static_cast<Stoner::Core::uint8>(State[Index] >> 8U);
        Digest.Bytes[Index * 4U + 3U] = static_cast<Stoner::Core::uint8>(State[Index]);
    }
    return Digest;
}

bool TryComputeRHISha256(
    std::span<const Stoner::Core::uint8> Bytes,
    FRHISha256Digest& OutDigest) noexcept
{
    OutDigest = {};
    try
    {
        OutDigest = ComputeRHISha256(Bytes);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    catch (const std::length_error&)
    {
        return false;
    }
}

bool ComputeRHINativeBindingMapDigest(
    const FRHINativeBindingMap& Map,
    FRHISha256Digest& OutDigest) noexcept
{
    OutDigest = {};
    try
    {
        Stoner::Core::TArray<Stoner::Core::uint8> Bytes;
        AppendText(Bytes, Map.PolicyVersion);
        AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Map.Entries.size()));
        for (const FRHINativeBindingEntry& Entry : Map.Entries)
        {
            AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Entry.Stage));
            AppendU32(Bytes, Entry.SetIndex);
            AppendU32(Bytes, Entry.BindingSlot);
            AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Entry.DescriptorType));
            AppendU32(Bytes, Entry.ArrayElement);
            AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Entry.NativeClass));
            AppendU32(Bytes, Entry.NativeIndex);
        }
        AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Map.ReservedRanges.size()));
        for (const FRHINativeBindingReservedRange& Range : Map.ReservedRanges)
        {
            AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Range.Stage));
            AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Range.NativeClass));
            AppendU32(Bytes, Range.FirstIndex);
            AppendU32(Bytes, Range.Count);
            AppendText(Bytes, Range.Purpose);
        }
        AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Map.LimitSnapshot.size()));
        for (const FRHINativeBindingLimit& Limit : Map.LimitSnapshot)
        {
            AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Limit.Stage));
            AppendU32(Bytes, static_cast<Stoner::Core::uint32>(Limit.NativeClass));
            AppendU32(Bytes, Limit.MaxCount);
        }
        return TryComputeRHISha256(Bytes, OutDigest);
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    catch (const std::length_error&)
    {
        return false;
    }
}

bool FinalizeRHINativeBindingMapDigest(FRHINativeBindingMap& Map) noexcept
{
    return ComputeRHINativeBindingMapDigest(Map, Map.CanonicalDigest);
}

bool IsCanonicalRHINativeBindingMap(const FRHINativeBindingMap& Map) noexcept
{
    if (Map.PolicyVersion.IsEmpty() || Map.LimitSnapshot.empty() ||
        !Map.CanonicalDigest.bAvailable ||
        !std::is_sorted(Map.Entries.begin(), Map.Entries.end(),
            [](const auto& Left, const auto& Right) { return SourceKey(Left) < SourceKey(Right); }) ||
        !std::is_sorted(Map.ReservedRanges.begin(), Map.ReservedRanges.end(),
            [](const auto& Left, const auto& Right) { return RangeKey(Left) < RangeKey(Right); }) ||
        !std::is_sorted(Map.LimitSnapshot.begin(), Map.LimitSnapshot.end(),
            [](const auto& Left, const auto& Right) { return LimitKey(Left) < LimitKey(Right); }))
    {
        return false;
    }

    for (std::size_t Index = 0; Index < Map.LimitSnapshot.size(); ++Index)
    {
        const FRHINativeBindingLimit& Limit = Map.LimitSnapshot[Index];
        if (!IsSupportedStage(Limit.Stage) ||
            !IsNativeClass(Limit.NativeClass) || Limit.MaxCount == 0 ||
            (Index > 0 && LimitKey(Map.LimitSnapshot[Index - 1U]) == LimitKey(Limit)))
        {
            return false;
        }
    }
    for (std::size_t Index = 0; Index < Map.ReservedRanges.size(); ++Index)
    {
        const FRHINativeBindingReservedRange& Range = Map.ReservedRanges[Index];
        if (!IsSupportedStage(Range.Stage) ||
            !IsNativeClass(Range.NativeClass) || Range.Count == 0 ||
            Range.Purpose.IsEmpty() ||
            static_cast<Stoner::Core::uint64>(Range.FirstIndex) + Range.Count >
                (Stoner::Core::uint64{1} << 32U) ||
            (Index > 0 && RangeKey(Map.ReservedRanges[Index - 1U]) == RangeKey(Range)))
        {
            return false;
        }
        if (Index > 0)
        {
            const auto& Previous = Map.ReservedRanges[Index - 1U];
            if (Previous.Stage == Range.Stage && Previous.NativeClass == Range.NativeClass &&
                static_cast<Stoner::Core::uint64>(Previous.FirstIndex) +
                        Previous.Count > Range.FirstIndex)
            {
                return false;
            }
        }
    }
    for (std::size_t Index = 0; Index < Map.Entries.size(); ++Index)
    {
        const FRHINativeBindingEntry& Entry = Map.Entries[Index];
        if (!IsSupportedStage(Entry.Stage) ||
            !IsNativeClass(Entry.NativeClass) ||
            !IsValidRHIDescriptorType(Entry.DescriptorType) ||
            !MatchesDescriptorClass(Entry.DescriptorType, Entry.NativeClass) ||
            (Index > 0 && SourceKey(Map.Entries[Index - 1U]) == SourceKey(Entry)))
        {
            return false;
        }
        for (std::size_t PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
        {
            if (NativeKey(Map.Entries[PreviousIndex]) == NativeKey(Entry))
            {
                return false;
            }
        }
        const auto Limit = std::find_if(
            Map.LimitSnapshot.begin(), Map.LimitSnapshot.end(),
            [&Entry](const FRHINativeBindingLimit& Candidate)
            {
                return Candidate.Stage == Entry.Stage &&
                    Candidate.NativeClass == Entry.NativeClass;
            });
        if (Limit == Map.LimitSnapshot.end() || Entry.NativeIndex >= Limit->MaxCount)
        {
            return false;
        }
        for (const FRHINativeBindingReservedRange& Range : Map.ReservedRanges)
        {
            if (Range.Stage == Entry.Stage && Range.NativeClass == Entry.NativeClass &&
                Entry.NativeIndex >= Range.FirstIndex &&
                static_cast<Stoner::Core::uint64>(Entry.NativeIndex) <
                    static_cast<Stoner::Core::uint64>(Range.FirstIndex) +
                        Range.Count)
            {
                return false;
            }
        }
    }

    FRHISha256Digest Computed;
    return ComputeRHINativeBindingMapDigest(Map, Computed) &&
        Computed == Map.CanonicalDigest;
}

} // namespace Stoner::RHI
