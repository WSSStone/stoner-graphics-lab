#include "Asset/FAssetDigest.h"

#include "Core/FPlatformHash.h"

#include <array>
#include <string>
#include <vector>

namespace Stoner::Asset
{
namespace
{

constexpr std::array<Core::uint32, 64> K = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

Core::uint32 RotateRight(Core::uint32 Value, Core::uint32 Count)
{
    return (Value >> Count) | (Value << (32U - Count));
}

int HexValue(char Character)
{
    if (Character >= '0' && Character <= '9')
    {
        return Character - '0';
    }
    if (Character >= 'a' && Character <= 'f')
    {
        return Character - 'a' + 10;
    }
    return -1;
}

} // namespace

FAssetDigest FAssetDigest::FromBytes(std::span<const Core::uint8> Bytes)
{
    FAssetDigest Digest;
    if (Core::FPlatformHash::TrySha256(Bytes, Digest.Bytes_))
    {
        Digest.Available_ = true;
        return Digest;
    }
    std::vector<Core::uint8> Message(Bytes.begin(), Bytes.end());
    const Core::uint64 BitLength = static_cast<Core::uint64>(Message.size()) * 8U;
    Message.push_back(0x80U);
    while ((Message.size() % 64U) != 56U)
    {
        Message.push_back(0U);
    }
    for (int Shift = 56; Shift >= 0; Shift -= 8)
    {
        Message.push_back(static_cast<Core::uint8>(BitLength >> Shift));
    }

    std::array<Core::uint32, 8> State = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    for (std::size_t Offset = 0; Offset < Message.size(); Offset += 64)
    {
        std::array<Core::uint32, 64> Words{};
        for (std::size_t Index = 0; Index < 16; ++Index)
        {
            const std::size_t Base = Offset + Index * 4;
            Words[Index] =
                (static_cast<Core::uint32>(Message[Base]) << 24U) |
                (static_cast<Core::uint32>(Message[Base + 1]) << 16U) |
                (static_cast<Core::uint32>(Message[Base + 2]) << 8U) |
                static_cast<Core::uint32>(Message[Base + 3]);
        }
        for (std::size_t Index = 16; Index < 64; ++Index)
        {
            const Core::uint32 S0 = RotateRight(Words[Index - 15], 7U) ^
                RotateRight(Words[Index - 15], 18U) ^ (Words[Index - 15] >> 3U);
            const Core::uint32 S1 = RotateRight(Words[Index - 2], 17U) ^
                RotateRight(Words[Index - 2], 19U) ^ (Words[Index - 2] >> 10U);
            Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
        }

        Core::uint32 A = State[0];
        Core::uint32 B = State[1];
        Core::uint32 C = State[2];
        Core::uint32 D = State[3];
        Core::uint32 E = State[4];
        Core::uint32 F = State[5];
        Core::uint32 G = State[6];
        Core::uint32 H = State[7];
        for (std::size_t Index = 0; Index < 64; ++Index)
        {
            const Core::uint32 S1 = RotateRight(E, 6U) ^ RotateRight(E, 11U) ^ RotateRight(E, 25U);
            const Core::uint32 Choice = (E & F) ^ ((~E) & G);
            const Core::uint32 Temp1 = H + S1 + Choice + K[Index] + Words[Index];
            const Core::uint32 S0 = RotateRight(A, 2U) ^ RotateRight(A, 13U) ^ RotateRight(A, 22U);
            const Core::uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const Core::uint32 Temp2 = S0 + Majority;
            H = G; G = F; F = E; E = D + Temp1;
            D = C; C = B; B = A; A = Temp1 + Temp2;
        }
        State[0] += A; State[1] += B; State[2] += C; State[3] += D;
        State[4] += E; State[5] += F; State[6] += G; State[7] += H;
    }

    Digest.Available_ = true;
    for (std::size_t Index = 0; Index < State.size(); ++Index)
    {
        Digest.Bytes_[Index * 4] = static_cast<Core::uint8>(State[Index] >> 24U);
        Digest.Bytes_[Index * 4 + 1] = static_cast<Core::uint8>(State[Index] >> 16U);
        Digest.Bytes_[Index * 4 + 2] = static_cast<Core::uint8>(State[Index] >> 8U);
        Digest.Bytes_[Index * 4 + 3] = static_cast<Core::uint8>(State[Index]);
    }
    return Digest;
}

EAssetResult FAssetDigest::ParseLowerHex(
    const Core::FString& Text,
    FAssetDigest& OutDigest) noexcept
{
    OutDigest = {};
    if (Text.Len() != 64)
    {
        return EAssetResult::InvalidInput;
    }
    FAssetDigest Parsed;
    Parsed.Available_ = true;
    for (std::size_t Index = 0; Index < Parsed.Bytes_.size(); ++Index)
    {
        const int High = HexValue(Text.View()[Index * 2]);
        const int Low = HexValue(Text.View()[Index * 2 + 1]);
        if (High < 0 || Low < 0)
        {
            return EAssetResult::InvalidInput;
        }
        Parsed.Bytes_[Index] = static_cast<Core::uint8>((High << 4) | Low);
    }
    OutDigest = Parsed;
    return EAssetResult::Success;
}

bool FAssetDigest::IsAvailable() const noexcept
{
    return Available_;
}

EAssetDigestAlgorithm FAssetDigest::GetAlgorithm() const noexcept
{
    return Algorithm_;
}

const std::array<Core::uint8, 32>& FAssetDigest::GetBytes() const noexcept
{
    return Bytes_;
}

Core::FString FAssetDigest::ToLowerHex() const
{
    if (!Available_)
    {
        return {};
    }
    constexpr char Digits[] = "0123456789abcdef";
    std::string Text;
    Text.resize(64);
    for (std::size_t Index = 0; Index < Bytes_.size(); ++Index)
    {
        Text[Index * 2] = Digits[Bytes_[Index] >> 4U];
        Text[Index * 2 + 1] = Digits[Bytes_[Index] & 0x0fU];
    }
    return Core::FString(std::move(Text));
}

} // namespace Stoner::Asset
