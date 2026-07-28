#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"

#include <array>
#include <span>

namespace Stoner::Asset
{

enum class EAssetDigestAlgorithm : Core::uint8
{
    Sha256
};

class FAssetDigest
{
public:
    FAssetDigest() = default;

    [[nodiscard]] static FAssetDigest FromBytes(std::span<const Core::uint8> Bytes);
    [[nodiscard]] static EAssetResult ParseLowerHex(
        const Core::FString& Text,
        FAssetDigest& OutDigest) noexcept;

    [[nodiscard]] bool IsAvailable() const noexcept;
    [[nodiscard]] EAssetDigestAlgorithm GetAlgorithm() const noexcept;
    [[nodiscard]] const std::array<Core::uint8, 32>& GetBytes() const noexcept;
    [[nodiscard]] Core::FString ToLowerHex() const;
    [[nodiscard]] bool operator==(const FAssetDigest&) const = default;

private:
    bool Available_ = false;
    EAssetDigestAlgorithm Algorithm_ = EAssetDigestAlgorithm::Sha256;
    std::array<Core::uint8, 32> Bytes_{};
};

} // namespace Stoner::Asset
