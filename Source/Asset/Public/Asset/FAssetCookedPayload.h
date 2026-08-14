#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

struct FAssetCookedPayloadLimits
{
    static constexpr Core::uint64 CompiledMaxEnvelopeBytes =
        1024ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint32 CompiledMaxHeaderBytes = 65535;

    Core::uint64 MaxEnvelopeBytes = CompiledMaxEnvelopeBytes;
    Core::uint64 MaxBodyBytes = CompiledMaxEnvelopeBytes;
    Core::uint32 MaxHeaderBytes = CompiledMaxHeaderBytes;

    [[nodiscard]] EAssetResult Validate() const noexcept;
};

struct FAssetCookedPayloadHeader
{
    static constexpr Core::uint16 CurrentContainerVersion = 1;

    Core::uint16 ContainerVersion = CurrentContainerVersion;
    Core::uint32 Flags = 0;
    FAssetId AssetId;
    Core::FString AssetType;
    Core::FString CodecId;
    Core::uint32 CodecVersion = 0;
    Core::uint32 PayloadSchemaVersion = 0;
    Core::uint64 BodyBytes = 0;
    FAssetDigest BodyDigest;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetCookedPayloadHeader&) const = default;
};

struct FAssetCookedPayloadEnvelope
{
    FAssetCookedPayloadHeader Header;
    Core::TArray<Core::uint8> ReservedHeaderExtensions;
    Core::TArray<Core::uint8> Body;
    FAssetDigest EnvelopeDigest;

    [[nodiscard]] bool operator==(const FAssetCookedPayloadEnvelope&) const = default;
};

} // namespace Stoner::Asset
