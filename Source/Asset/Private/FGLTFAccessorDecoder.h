#pragma once

#include "Asset/EAssetResult.h"
#include "Core/TArray.h"

#include <optional>
#include <span>

namespace Stoner::Asset::Private
{

enum class EGLTFComponentType : Core::uint32
{
    Int8 = 5120,
    UInt8 = 5121,
    Int16 = 5122,
    UInt16 = 5123,
    UInt32 = 5125,
    Float32 = 5126
};

enum class EGLTFAccessorType : Core::uint8
{
    Scalar = 1,
    Vec2 = 2,
    Vec3 = 3,
    Vec4 = 4
};

struct FGLTFSparseAccessorSource
{
    Core::uint32 Count = 0;
    EGLTFComponentType IndexComponentType = EGLTFComponentType::UInt8;
    std::span<const Core::uint8> Indices;
    Core::uint64 IndicesByteOffset = 0;
    std::span<const Core::uint8> Values;
    Core::uint64 ValuesByteOffset = 0;
};

struct FGLTFAccessorSource
{
    EGLTFComponentType ComponentType = EGLTFComponentType::Float32;
    EGLTFAccessorType Type = EGLTFAccessorType::Scalar;
    Core::uint32 Count = 0;
    bool bNormalized = false;
    bool bHasBaseBufferView = true;
    std::span<const Core::uint8> BaseBytes;
    Core::uint64 ByteOffset = 0;
    Core::uint64 ByteStride = 0;
    std::optional<FGLTFSparseAccessorSource> Sparse;
};

struct FGLTFDecodedAccessor
{
    Core::uint32 ElementCount = 0;
    Core::uint32 ComponentCount = 0;
    Core::TArray<float> Values;
};

[[nodiscard]] EAssetResult DecodeGLTFAccessorToFloat(
    const FGLTFAccessorSource& Source,
    Core::uint32 MaximumElements,
    FGLTFDecodedAccessor& OutAccessor);

[[nodiscard]] EAssetResult DecodeGLTFAccessorToIndices(
    const FGLTFAccessorSource& Source,
    Core::uint32 MaximumElements,
    Core::TArray<Core::uint32>& OutIndices);

} // namespace Stoner::Asset::Private
