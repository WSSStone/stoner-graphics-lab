#include "Asset/FShaderNativeBindingEvidence.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <tuple>

namespace Stoner::Asset
{
namespace
{

void U32(Core::TArray<Core::uint8>& Out, Core::uint32 Value)
{
    Out.push_back(static_cast<Core::uint8>(Value));
    Out.push_back(static_cast<Core::uint8>(Value >> 8U));
    Out.push_back(static_cast<Core::uint8>(Value >> 16U));
    Out.push_back(static_cast<Core::uint8>(Value >> 24U));
}

void Text(Core::TArray<Core::uint8>& Out, const Core::FString& Value)
{
    U32(Out, static_cast<Core::uint32>(Value.Len()));
    Out.insert(Out.end(), Value.View().begin(), Value.View().end());
}

auto SourceKey(const FShaderNativeBindingEntry& Value)
{
    return std::tuple(Value.Stage, Value.SetIndex, Value.BindingIndex,
        Value.DescriptorType, Value.ArrayElement, Value.NativeClass);
}

auto NativeKey(const FShaderNativeBindingEntry& Value)
{
    return std::tuple(Value.Stage, Value.NativeClass, Value.NativeIndex);
}

auto RangeKey(const FShaderNativeReservedRange& Value)
{
    return std::tuple(Value.Stage, Value.NativeClass, Value.FirstIndex);
}

auto LimitKey(const FShaderNativeBindingLimit& Value)
{
    return std::tuple(Value.Stage, Value.NativeClass);
}

bool ClassMatches(
    EShaderResourceKind Type,
    EShaderNativeResourceClass NativeClass)
{
    if (Type == EShaderResourceKind::UniformBuffer ||
        Type == EShaderResourceKind::StorageBuffer)
        return NativeClass == EShaderNativeResourceClass::Buffer;
    if (Type == EShaderResourceKind::SampledTexture ||
        Type == EShaderResourceKind::StorageTexture)
        return NativeClass == EShaderNativeResourceClass::Texture;
    if (Type == EShaderResourceKind::CombinedTextureSampler)
        return NativeClass == EShaderNativeResourceClass::Texture ||
            NativeClass == EShaderNativeResourceClass::Sampler;
    return Type == EShaderResourceKind::Sampler &&
        NativeClass == EShaderNativeResourceClass::Sampler;
}

bool IsSupportedStage(EShaderStage Stage)
{
    return Stage == EShaderStage::Vertex ||
        Stage == EShaderStage::Fragment || Stage == EShaderStage::Compute;
}

bool IsNativeClass(EShaderNativeResourceClass NativeClass)
{
    return NativeClass == EShaderNativeResourceClass::Buffer ||
        NativeClass == EShaderNativeResourceClass::Texture ||
        NativeClass == EShaderNativeResourceClass::Sampler;
}

Core::uint32 CanonicalStage(EShaderStage Stage)
{
    switch (Stage)
    {
    case EShaderStage::Vertex: return 1;
    case EShaderStage::Fragment: return 2;
    case EShaderStage::Compute: return 3;
    }
    return std::numeric_limits<Core::uint32>::max();
}

Core::uint32 CanonicalDescriptorType(EShaderResourceKind Type)
{
    switch (Type)
    {
    case EShaderResourceKind::UniformBuffer: return 0;
    case EShaderResourceKind::StorageBuffer: return 1;
    case EShaderResourceKind::SampledTexture: return 2;
    case EShaderResourceKind::StorageTexture: return 3;
    case EShaderResourceKind::Sampler: return 4;
    case EShaderResourceKind::CombinedTextureSampler: return 5;
    }
    return std::numeric_limits<Core::uint32>::max();
}

EAssetResult ComputeDigest(
    const FShaderNativeBindingEvidence& Evidence,
    FAssetDigest& Out) noexcept
{
    Out = {};
    try
    {
        Core::TArray<Core::uint8> Bytes;
        Text(Bytes, Evidence.PolicyVersion);
        U32(Bytes, static_cast<Core::uint32>(Evidence.Entries.size()));
        for (const auto& Entry : Evidence.Entries)
        {
            U32(Bytes, CanonicalStage(Entry.Stage));
            U32(Bytes, Entry.SetIndex);
            U32(Bytes, Entry.BindingIndex);
            U32(Bytes, CanonicalDescriptorType(Entry.DescriptorType));
            U32(Bytes, Entry.ArrayElement);
            U32(Bytes, static_cast<Core::uint32>(Entry.NativeClass));
            U32(Bytes, Entry.NativeIndex);
        }
        U32(Bytes, static_cast<Core::uint32>(Evidence.ReservedRanges.size()));
        for (const auto& Range : Evidence.ReservedRanges)
        {
            U32(Bytes, CanonicalStage(Range.Stage));
            U32(Bytes, static_cast<Core::uint32>(Range.NativeClass));
            U32(Bytes, Range.FirstIndex);
            U32(Bytes, Range.Count);
            Text(Bytes, Range.Purpose);
        }
        U32(Bytes, static_cast<Core::uint32>(Evidence.LimitSnapshot.size()));
        for (const auto& Limit : Evidence.LimitSnapshot)
        {
            U32(Bytes, CanonicalStage(Limit.Stage));
            U32(Bytes, static_cast<Core::uint32>(Limit.NativeClass));
            U32(Bytes, Limit.MaxCount);
        }
        Out = FAssetDigest::FromBytes(Bytes);
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        return EAssetResult::CapacityExceeded;
    }
    catch (const std::length_error&)
    {
        return EAssetResult::CapacityExceeded;
    }
}

} // namespace

EAssetResult FShaderNativeBindingEvidence::Validate() const noexcept
{
    if (PolicyVersion != Core::FString("metal-direct-binding-v1") ||
        LimitSnapshot.empty() ||
        !CanonicalDigest.IsAvailable() ||
        !std::is_sorted(Entries.begin(), Entries.end(),
            [](const auto& A, const auto& B) { return SourceKey(A) < SourceKey(B); }) ||
        !std::is_sorted(ReservedRanges.begin(), ReservedRanges.end(),
            [](const auto& A, const auto& B) { return RangeKey(A) < RangeKey(B); }) ||
        !std::is_sorted(LimitSnapshot.begin(), LimitSnapshot.end(),
            [](const auto& A, const auto& B) { return LimitKey(A) < LimitKey(B); }))
        return EAssetResult::InvalidInput;

    for (Core::usize Index = 0; Index < LimitSnapshot.size(); ++Index)
    {
        if (!IsSupportedStage(LimitSnapshot[Index].Stage) ||
            !IsNativeClass(LimitSnapshot[Index].NativeClass) ||
            LimitSnapshot[Index].MaxCount == 0 ||
            (Index > 0 && LimitKey(LimitSnapshot[Index - 1]) == LimitKey(LimitSnapshot[Index])))
            return EAssetResult::InvalidInput;
    }
    for (Core::usize Index = 0; Index < ReservedRanges.size(); ++Index)
    {
        const auto& Range = ReservedRanges[Index];
        if (!IsSupportedStage(Range.Stage) ||
            !IsNativeClass(Range.NativeClass) ||
            Range.Count == 0 || Range.Purpose.IsEmpty() ||
            static_cast<Core::uint64>(Range.FirstIndex) + Range.Count >
                (Core::uint64{1} << 32U) ||
            (Index > 0 && RangeKey(ReservedRanges[Index - 1]) == RangeKey(Range)))
            return EAssetResult::InvalidInput;
        if (Index > 0)
        {
            const auto& Previous = ReservedRanges[Index - 1];
            if (Previous.Stage == Range.Stage &&
                Previous.NativeClass == Range.NativeClass &&
                static_cast<Core::uint64>(Previous.FirstIndex) +
                        Previous.Count > Range.FirstIndex)
                return EAssetResult::InvalidInput;
        }
    }
    for (Core::usize Index = 0; Index < Entries.size(); ++Index)
    {
        const auto& Entry = Entries[Index];
        if (!IsSupportedStage(Entry.Stage) ||
            !IsNativeClass(Entry.NativeClass) ||
            !ClassMatches(Entry.DescriptorType, Entry.NativeClass) ||
            (Index > 0 && SourceKey(Entries[Index - 1]) == SourceKey(Entry)))
            return EAssetResult::InvalidInput;
        for (Core::usize Previous = 0; Previous < Index; ++Previous)
            if (NativeKey(Entries[Previous]) == NativeKey(Entry))
                return EAssetResult::InvalidInput;
        const auto Limit = std::find_if(
            LimitSnapshot.begin(), LimitSnapshot.end(),
            [&Entry](const auto& Value)
            {
                return Value.Stage == Entry.Stage &&
                    Value.NativeClass == Entry.NativeClass;
            });
        if (Limit == LimitSnapshot.end() || Entry.NativeIndex >= Limit->MaxCount)
            return EAssetResult::InvalidInput;
        for (const auto& Range : ReservedRanges)
            if (Range.Stage == Entry.Stage && Range.NativeClass == Entry.NativeClass &&
                Entry.NativeIndex >= Range.FirstIndex &&
                static_cast<Core::uint64>(Entry.NativeIndex) <
                    static_cast<Core::uint64>(Range.FirstIndex) + Range.Count)
                return EAssetResult::InvalidInput;
    }
    FAssetDigest Digest;
    return ComputeDigest(*this, Digest) == EAssetResult::Success &&
            Digest == CanonicalDigest
        ? EAssetResult::Success
        : EAssetResult::InvalidInput;
}

EAssetResult FinalizeShaderNativeBindingEvidence(
    FShaderNativeBindingEvidence& Evidence) noexcept
{
    const EAssetResult Result = ComputeDigest(Evidence, Evidence.CanonicalDigest);
    return Result == EAssetResult::Success ? Evidence.Validate() : Result;
}

} // namespace Stoner::Asset
