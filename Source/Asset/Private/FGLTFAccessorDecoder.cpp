#include "FGLTFAccessorDecoder.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace Stoner::Asset::Private
{
namespace
{

Core::uint32 ComponentSize(EGLTFComponentType Type) noexcept
{
    switch (Type)
    {
    case EGLTFComponentType::Int8:
    case EGLTFComponentType::UInt8: return 1;
    case EGLTFComponentType::Int16:
    case EGLTFComponentType::UInt16: return 2;
    case EGLTFComponentType::UInt32:
    case EGLTFComponentType::Float32: return 4;
    }
    return 0;
}

Core::uint32 ComponentCount(EGLTFAccessorType Type) noexcept
{
    return static_cast<Core::uint32>(Type);
}

bool CheckedAdd(Core::uint64 Left, Core::uint64 Right, Core::uint64& Out) noexcept
{
    if (Right > std::numeric_limits<Core::uint64>::max() - Left)
    {
        return false;
    }
    Out = Left + Right;
    return true;
}

bool CheckedMultiply(Core::uint64 Left, Core::uint64 Right, Core::uint64& Out) noexcept
{
    if (Left != 0 && Right > std::numeric_limits<Core::uint64>::max() / Left)
    {
        return false;
    }
    Out = Left * Right;
    return true;
}

bool InRange(
    std::span<const Core::uint8> Bytes,
    Core::uint64 Offset,
    Core::uint64 Length) noexcept
{
    return Offset <= Bytes.size() && Length <= Bytes.size() - Offset;
}

bool IsUnsignedIndexType(EGLTFComponentType Type) noexcept
{
    return Type == EGLTFComponentType::UInt8 ||
        Type == EGLTFComponentType::UInt16 ||
        Type == EGLTFComponentType::UInt32;
}

bool IsSourceLayoutValid(
    const FGLTFAccessorSource& Source,
    Core::uint32 MaximumElements,
    Core::uint32& OutComponentCount,
    Core::uint32& OutComponentSize,
    Core::uint64& OutElementSize,
    Core::uint64& OutStride) noexcept
{
    OutComponentCount = ComponentCount(Source.Type);
    OutComponentSize = ComponentSize(Source.ComponentType);
    if (Source.Count == 0 || Source.Count > MaximumElements ||
        OutComponentCount == 0 || OutComponentCount > 4 || OutComponentSize == 0 ||
        !CheckedMultiply(OutComponentCount, OutComponentSize, OutElementSize))
    {
        return false;
    }
    OutStride = Source.ByteStride == 0 ? OutElementSize : Source.ByteStride;
    if (OutStride < OutElementSize ||
        Source.ByteOffset % OutComponentSize != 0 ||
        OutStride % OutComponentSize != 0)
    {
        return false;
    }
    if (!Source.bHasBaseBufferView)
    {
        return Source.ByteOffset == 0 && Source.ByteStride == 0;
    }
    Core::uint64 LastOffset = 0;
    Core::uint64 Required = 0;
    return CheckedMultiply(Source.Count - 1, OutStride, LastOffset) &&
        CheckedAdd(Source.ByteOffset, LastOffset, LastOffset) &&
        CheckedAdd(LastOffset, OutElementSize, Required) &&
        InRange(Source.BaseBytes, 0, Required);
}

bool ReadUnsigned(
    std::span<const Core::uint8> Bytes,
    Core::uint64 Offset,
    EGLTFComponentType Type,
    Core::uint32& OutValue) noexcept
{
    const Core::uint32 Size = ComponentSize(Type);
    if (!IsUnsignedIndexType(Type) || !InRange(Bytes, Offset, Size))
    {
        return false;
    }
    switch (Type)
    {
    case EGLTFComponentType::UInt8:
        OutValue = Bytes[Offset];
        return true;
    case EGLTFComponentType::UInt16:
        OutValue = static_cast<Core::uint32>(Bytes[Offset]) |
            (static_cast<Core::uint32>(Bytes[Offset + 1]) << 8U);
        return true;
    case EGLTFComponentType::UInt32:
        OutValue = static_cast<Core::uint32>(Bytes[Offset]) |
            (static_cast<Core::uint32>(Bytes[Offset + 1]) << 8U) |
            (static_cast<Core::uint32>(Bytes[Offset + 2]) << 16U) |
            (static_cast<Core::uint32>(Bytes[Offset + 3]) << 24U);
        return true;
    default:
        return false;
    }
}

bool ReadFloat(
    std::span<const Core::uint8> Bytes,
    Core::uint64 Offset,
    EGLTFComponentType Type,
    bool bNormalized,
    float& OutValue) noexcept
{
    const Core::uint32 Size = ComponentSize(Type);
    if (Size == 0 || !InRange(Bytes, Offset, Size))
    {
        return false;
    }
    switch (Type)
    {
    case EGLTFComponentType::Int8:
    {
        const auto Value = static_cast<Core::int8>(Bytes[Offset]);
        OutValue = bNormalized
            ? std::max(static_cast<float>(Value) / 127.0f, -1.0f)
            : static_cast<float>(Value);
        return true;
    }
    case EGLTFComponentType::UInt8:
        OutValue = bNormalized ? Bytes[Offset] / 255.0f : Bytes[Offset];
        return true;
    case EGLTFComponentType::Int16:
    {
        const Core::uint16 Raw = static_cast<Core::uint16>(Bytes[Offset]) |
            (static_cast<Core::uint16>(Bytes[Offset + 1]) << 8U);
        const auto Value = static_cast<Core::int16>(Raw);
        OutValue = bNormalized
            ? std::max(static_cast<float>(Value) / 32767.0f, -1.0f)
            : static_cast<float>(Value);
        return true;
    }
    case EGLTFComponentType::UInt16:
    {
        const Core::uint16 Value = static_cast<Core::uint16>(Bytes[Offset]) |
            (static_cast<Core::uint16>(Bytes[Offset + 1]) << 8U);
        OutValue = bNormalized ? Value / 65535.0f : static_cast<float>(Value);
        return true;
    }
    case EGLTFComponentType::UInt32:
    {
        Core::uint32 Value = 0;
        if (!ReadUnsigned(Bytes, Offset, Type, Value)) return false;
        OutValue = bNormalized
            ? static_cast<float>(static_cast<double>(Value) / 4294967295.0)
            : static_cast<float>(Value);
        return true;
    }
    case EGLTFComponentType::Float32:
        if (bNormalized) return false;
        std::memcpy(&OutValue, Bytes.data() + Offset, sizeof(OutValue));
        return true;
    }
    return false;
}

bool DecodeSparse(
    const FGLTFAccessorSource& Source,
    Core::uint32 ComponentCountValue,
    Core::uint32 ComponentSizeValue,
    Core::uint64 ElementSize,
    FGLTFDecodedAccessor& OutAccessor) noexcept
{
    if (!Source.Sparse)
    {
        return true;
    }
    const FGLTFSparseAccessorSource& Sparse = *Source.Sparse;
    const Core::uint32 IndexSize = ComponentSize(Sparse.IndexComponentType);
    if (Sparse.Count > Source.Count || !IsUnsignedIndexType(Sparse.IndexComponentType) ||
        Sparse.IndicesByteOffset % IndexSize != 0 ||
        Sparse.ValuesByteOffset % ComponentSizeValue != 0)
    {
        return false;
    }
    Core::uint64 IndexBytes = 0;
    Core::uint64 ValueBytes = 0;
    if (!CheckedMultiply(Sparse.Count, IndexSize, IndexBytes) ||
        !CheckedMultiply(Sparse.Count, ElementSize, ValueBytes) ||
        !InRange(Sparse.Indices, Sparse.IndicesByteOffset, IndexBytes) ||
        !InRange(Sparse.Values, Sparse.ValuesByteOffset, ValueBytes))
    {
        return false;
    }
    Core::uint32 Previous = 0;
    Core::uint32 LastIndex = 0;
    bool bHasPrevious = false;
    for (Core::uint32 SparseIndex = 0; SparseIndex < Sparse.Count; ++SparseIndex)
    {
        Core::uint64 IndexOffset = 0;
        Core::uint64 ValueOffset = 0;
        if (!CheckedMultiply(SparseIndex, IndexSize, IndexOffset) ||
            !CheckedAdd(Sparse.IndicesByteOffset, IndexOffset, IndexOffset) ||
            !ReadUnsigned(Sparse.Indices, IndexOffset, Sparse.IndexComponentType, Previous))
        {
            return false;
        }
        if (Previous >= Source.Count ||
            (bHasPrevious && Previous <= LastIndex))
        {
            return false;
        }
        LastIndex = Previous;
        bHasPrevious = true;
        if (!CheckedMultiply(SparseIndex, ElementSize, ValueOffset) ||
            !CheckedAdd(Sparse.ValuesByteOffset, ValueOffset, ValueOffset))
        {
            return false;
        }
        for (Core::uint32 Component = 0; Component < ComponentCountValue; ++Component)
        {
            float Value = 0.0f;
            if (!ReadFloat(
                    Sparse.Values, ValueOffset + Component * ComponentSizeValue,
                    Source.ComponentType, Source.bNormalized, Value))
            {
                return false;
            }
            OutAccessor.Values[Previous * ComponentCountValue + Component] = Value;
        }
    }
    return true;
}

bool DecodeSparseIndices(
    const FGLTFAccessorSource& Source,
    Core::TArray<Core::uint32>& OutIndices) noexcept
{
    if (!Source.Sparse)
    {
        return true;
    }
    const FGLTFSparseAccessorSource& Sparse = *Source.Sparse;
    const Core::uint32 SparseIndexSize = ComponentSize(Sparse.IndexComponentType);
    const Core::uint32 ValueSize = ComponentSize(Source.ComponentType);
    if (Sparse.Count > Source.Count ||
        !IsUnsignedIndexType(Sparse.IndexComponentType) ||
        SparseIndexSize == 0 || ValueSize == 0 ||
        Sparse.IndicesByteOffset % SparseIndexSize != 0 ||
        Sparse.ValuesByteOffset % ValueSize != 0)
    {
        return false;
    }
    Core::uint64 IndicesLength = 0;
    Core::uint64 ValuesLength = 0;
    if (!CheckedMultiply(Sparse.Count, SparseIndexSize, IndicesLength) ||
        !CheckedMultiply(Sparse.Count, ValueSize, ValuesLength) ||
        !InRange(Sparse.Indices, Sparse.IndicesByteOffset, IndicesLength) ||
        !InRange(Sparse.Values, Sparse.ValuesByteOffset, ValuesLength))
    {
        return false;
    }
    Core::uint32 Previous = 0;
    bool bHasPrevious = false;
    for (Core::uint32 SparseElement = 0;
         SparseElement < Sparse.Count;
         ++SparseElement)
    {
        Core::uint32 TargetIndex = 0;
        Core::uint32 Value = 0;
        const Core::uint64 IndexOffset = Sparse.IndicesByteOffset +
            static_cast<Core::uint64>(SparseElement) * SparseIndexSize;
        const Core::uint64 ValueOffset = Sparse.ValuesByteOffset +
            static_cast<Core::uint64>(SparseElement) * ValueSize;
        if (!ReadUnsigned(
                Sparse.Indices, IndexOffset, Sparse.IndexComponentType, TargetIndex) ||
            !ReadUnsigned(Sparse.Values, ValueOffset, Source.ComponentType, Value) ||
            TargetIndex >= Source.Count ||
            (bHasPrevious && TargetIndex <= Previous))
        {
            return false;
        }
        OutIndices[TargetIndex] = Value;
        Previous = TargetIndex;
        bHasPrevious = true;
    }
    return true;
}

} // namespace

EAssetResult DecodeGLTFAccessorToFloat(
    const FGLTFAccessorSource& Source,
    Core::uint32 MaximumElements,
    FGLTFDecodedAccessor& OutAccessor)
{
    OutAccessor = {};
    Core::uint32 Components = 0;
    Core::uint32 Size = 0;
    Core::uint64 ElementSize = 0;
    Core::uint64 Stride = 0;
    if (!IsSourceLayoutValid(Source, MaximumElements, Components, Size, ElementSize, Stride))
    {
        return EAssetResult::MalformedSource;
    }
    Core::uint64 ValueCount = 0;
    if (!CheckedMultiply(Source.Count, Components, ValueCount) ||
        ValueCount > std::numeric_limits<Core::usize>::max())
    {
        return EAssetResult::CapacityExceeded;
    }
    OutAccessor.ElementCount = Source.Count;
    OutAccessor.ComponentCount = Components;
    OutAccessor.Values.assign(static_cast<Core::usize>(ValueCount), 0.0f);
    if (Source.bHasBaseBufferView)
    {
        for (Core::uint32 Element = 0; Element < Source.Count; ++Element)
        {
            const Core::uint64 ElementOffset = Source.ByteOffset + Element * Stride;
            for (Core::uint32 Component = 0; Component < Components; ++Component)
            {
                if (!ReadFloat(
                        Source.BaseBytes, ElementOffset + Component * Size,
                        Source.ComponentType, Source.bNormalized,
                        OutAccessor.Values[Element * Components + Component]))
                {
                    OutAccessor = {};
                    return EAssetResult::MalformedSource;
                }
            }
        }
    }
    if (!DecodeSparse(Source, Components, Size, ElementSize, OutAccessor))
    {
        OutAccessor = {};
        return EAssetResult::MalformedSource;
    }
    return EAssetResult::Success;
}

EAssetResult DecodeGLTFAccessorToIndices(
    const FGLTFAccessorSource& Source,
    Core::uint32 MaximumElements,
    Core::TArray<Core::uint32>& OutIndices)
{
    OutIndices.clear();
    if (Source.Type != EGLTFAccessorType::Scalar || Source.bNormalized ||
        !IsUnsignedIndexType(Source.ComponentType))
    {
        return EAssetResult::MalformedSource;
    }
    Core::uint32 Components = 0;
    Core::uint32 Size = 0;
    Core::uint64 ElementSize = 0;
    Core::uint64 Stride = 0;
    if (!IsSourceLayoutValid(Source, MaximumElements, Components, Size, ElementSize, Stride))
    {
        return EAssetResult::MalformedSource;
    }
    OutIndices.assign(Source.Count, 0);
    if (Source.bHasBaseBufferView)
    {
        for (Core::uint32 Index = 0; Index < Source.Count; ++Index)
        {
            if (!ReadUnsigned(
                    Source.BaseBytes, Source.ByteOffset + Index * Stride,
                    Source.ComponentType, OutIndices[Index]))
            {
                OutIndices.clear();
                return EAssetResult::MalformedSource;
            }
        }
    }
    if (!DecodeSparseIndices(Source, OutIndices))
    {
        OutIndices.clear();
        return EAssetResult::MalformedSource;
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
