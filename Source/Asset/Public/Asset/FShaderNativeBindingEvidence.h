#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FMaterialShaderTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

enum class EShaderNativeResourceClass : Core::uint8
{
    Buffer,
    Texture,
    Sampler
};

struct FShaderNativeBindingEntry
{
    EShaderStage Stage = EShaderStage::Vertex;
    Core::uint32 SetIndex = 0;
    Core::uint32 BindingIndex = 0;
    EShaderResourceKind DescriptorType = EShaderResourceKind::UniformBuffer;
    Core::uint32 ArrayElement = 0;
    EShaderNativeResourceClass NativeClass = EShaderNativeResourceClass::Buffer;
    Core::uint32 NativeIndex = 0;

    [[nodiscard]] bool operator==(const FShaderNativeBindingEntry&) const = default;
};

struct FShaderNativeReservedRange
{
    EShaderStage Stage = EShaderStage::Vertex;
    EShaderNativeResourceClass NativeClass = EShaderNativeResourceClass::Buffer;
    Core::uint32 FirstIndex = 0;
    Core::uint32 Count = 0;
    Core::FString Purpose;

    [[nodiscard]] bool operator==(const FShaderNativeReservedRange&) const = default;
};

struct FShaderNativeBindingLimit
{
    EShaderStage Stage = EShaderStage::Vertex;
    EShaderNativeResourceClass NativeClass = EShaderNativeResourceClass::Buffer;
    Core::uint32 MaxCount = 0;

    [[nodiscard]] bool operator==(const FShaderNativeBindingLimit&) const = default;
};

struct FShaderNativeBindingEvidence
{
    Core::FString PolicyVersion;
    Core::TArray<FShaderNativeBindingEntry> Entries;
    Core::TArray<FShaderNativeReservedRange> ReservedRanges;
    Core::TArray<FShaderNativeBindingLimit> LimitSnapshot;
    FAssetDigest CanonicalDigest;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FShaderNativeBindingEvidence&) const = default;
};

[[nodiscard]] EAssetResult FinalizeShaderNativeBindingEvidence(
    FShaderNativeBindingEvidence& Evidence) noexcept;

} // namespace Stoner::Asset
