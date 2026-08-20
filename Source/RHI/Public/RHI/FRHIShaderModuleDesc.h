#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIShaderPayloadFormat.h"
#include "RHI/ERHIDescriptorType.h"
#include "RHI/ERHIShaderStage.h"
#include "RHI/ERHIRuntimeMode.h"
#include "RHI/FRHINativeBindingMap.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace Stoner::RHI
{

enum class ERHIShaderBytecodeValidationMode
{
    StructuralFallback,
    Runtime
};

enum class ERHIPipelineReuseState
{
    NotReusable,
    Created,
    Reused,
    Rejected,
    Invalidated,
    Unavailable
};

struct FRHIShaderPayloadDesc
{
    ERHIShaderPayloadFormat Format = ERHIShaderPayloadFormat::Unknown;
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes;
    Stoner::Core::FString PayloadIdentity;
    Stoner::Core::FString TargetProfile;
    FRHISha256Digest PayloadDigest;
};

struct FRHIShaderInterfaceBinding
{
    Stoner::Core::uint32 SetIndex = 0;
    Stoner::Core::uint32 BindingSlot = 0;
    ERHIDescriptorType DescriptorType = ERHIDescriptorType::UniformBuffer;
    Stoner::Core::uint32 ArrayCount = 1;
    ERHIShaderStageFlags Visibility = ERHIShaderStageFlags::None;
};

struct FRHIShaderConstantRange
{
    Stoner::Core::uint32 OffsetBytes = 0;
    Stoner::Core::uint32 SizeBytes = 0;
    ERHIShaderStageFlags Visibility = ERHIShaderStageFlags::None;
};

struct FRHIShaderInterfaceMetadata
{
    Stoner::Core::TArray<FRHIShaderInterfaceBinding> Bindings;
    Stoner::Core::TArray<FRHIShaderConstantRange> ConstantRanges;
    Stoner::Core::FString DebugName;
};

struct FRHIShaderModuleDesc
{
    ERHIShaderStage Stage = ERHIShaderStage::Unknown;
    Stoner::Core::FString EntryPoint;
    FRHIShaderPayloadDesc Payload;
    FRHIShaderInterfaceMetadata InterfaceMetadata;
    FRHINativeBindingMap NativeBindingMap;
    ERHIShaderBytecodeValidationMode ValidationMode = ERHIShaderBytecodeValidationMode::StructuralFallback;
    ERHIRuntimeObjectMode RuntimeMode = ERHIRuntimeObjectMode::Unknown;
    Stoner::Core::FString DebugName;
};

[[nodiscard]] constexpr ERHIShaderStageFlags ToShaderStageFlag(ERHIShaderStage Stage) noexcept
{
    switch (Stage)
    {
    case ERHIShaderStage::Vertex:
        return ERHIShaderStageFlags::Vertex;
    case ERHIShaderStage::Fragment:
        return ERHIShaderStageFlags::Fragment;
    case ERHIShaderStage::Compute:
        return ERHIShaderStageFlags::Compute;
    case ERHIShaderStage::Geometry:
        return ERHIShaderStageFlags::Geometry;
    case ERHIShaderStage::TessellationControl:
        return ERHIShaderStageFlags::TessellationControl;
    case ERHIShaderStage::TessellationEvaluation:
        return ERHIShaderStageFlags::TessellationEvaluation;
    case ERHIShaderStage::Mesh:
        return ERHIShaderStageFlags::Mesh;
    case ERHIShaderStage::Task:
        return ERHIShaderStageFlags::Task;
    case ERHIShaderStage::RayGeneration:
    case ERHIShaderStage::AnyHit:
    case ERHIShaderStage::ClosestHit:
    case ERHIShaderStage::Miss:
        return ERHIShaderStageFlags::RayTracing;
    default:
        return ERHIShaderStageFlags::None;
    }
}

[[nodiscard]] inline bool IsSupportedRHIShaderStage(ERHIShaderStage Stage) noexcept
{
    return Stage == ERHIShaderStage::Vertex || Stage == ERHIShaderStage::Fragment || Stage == ERHIShaderStage::Compute;
}

[[nodiscard]] inline Stoner::Core::uint32 ReadRHIShaderSpirvWord(
    const FRHIShaderPayloadDesc& Payload,
    std::size_t WordIndex) noexcept
{
    const std::size_t Offset = WordIndex * sizeof(Stoner::Core::uint32);
    return static_cast<Stoner::Core::uint32>(Payload.Bytes[Offset]) |
        (static_cast<Stoner::Core::uint32>(Payload.Bytes[Offset + 1U]) << 8U) |
        (static_cast<Stoner::Core::uint32>(Payload.Bytes[Offset + 2U]) << 16U) |
        (static_cast<Stoner::Core::uint32>(Payload.Bytes[Offset + 3U]) << 24U);
}

[[nodiscard]] inline bool TryGetRHIShaderSpirvWords(
    const FRHIShaderPayloadDesc& Payload,
    Stoner::Core::TArray<Stoner::Core::uint32>& OutWords) noexcept
{
    OutWords.clear();
    if (Payload.Format != ERHIShaderPayloadFormat::SPIRV || Payload.Bytes.empty() ||
        Payload.Bytes.size() % sizeof(Stoner::Core::uint32) != 0)
    {
        return false;
    }
    try
    {
        OutWords.resize(Payload.Bytes.size() / sizeof(Stoner::Core::uint32));
        for (std::size_t Index = 0; Index < OutWords.size(); ++Index)
        {
            OutWords[Index] = ReadRHIShaderSpirvWord(Payload, Index);
        }
        return true;
    }
    catch (const std::bad_alloc&)
    {
        OutWords.clear();
        return false;
    }
    catch (const std::length_error&)
    {
        OutWords.clear();
        return false;
    }
}

[[nodiscard]] inline bool SetRHIShaderSpirvWords(
    FRHIShaderPayloadDesc& OutPayload,
    const Stoner::Core::TArray<Stoner::Core::uint32>& Words,
    const Stoner::Core::FString& PayloadIdentity,
    const Stoner::Core::FString& TargetProfile = "legacy-vulkan-v1") noexcept
{
    try
    {
        OutPayload = {};
        OutPayload.Format = ERHIShaderPayloadFormat::SPIRV;
        OutPayload.PayloadIdentity = PayloadIdentity;
        OutPayload.TargetProfile = TargetProfile;
        OutPayload.Bytes.resize(Words.size() * sizeof(Stoner::Core::uint32));
        for (std::size_t Index = 0; Index < Words.size(); ++Index)
        {
            const Stoner::Core::uint32 Word = Words[Index];
            const std::size_t Offset = Index * sizeof(Stoner::Core::uint32);
            OutPayload.Bytes[Offset] = static_cast<Stoner::Core::uint8>(Word);
            OutPayload.Bytes[Offset + 1U] = static_cast<Stoner::Core::uint8>(Word >> 8U);
            OutPayload.Bytes[Offset + 2U] = static_cast<Stoner::Core::uint8>(Word >> 16U);
            OutPayload.Bytes[Offset + 3U] = static_cast<Stoner::Core::uint8>(Word >> 24U);
        }
        OutPayload.PayloadDigest = ComputeRHISha256(OutPayload.Bytes);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        OutPayload = {};
        return false;
    }
    catch (const std::length_error&)
    {
        OutPayload = {};
        return false;
    }
}

[[nodiscard]] inline bool IsValidRHIShaderPayload(
    const FRHIShaderPayloadDesc& Payload) noexcept
{
    FRHISha256Digest ComputedDigest;
    if (!IsValidRHIShaderPayloadFormat(Payload.Format) || Payload.Bytes.empty() ||
        Payload.PayloadIdentity.IsEmpty() || Payload.TargetProfile.IsEmpty() ||
        !Payload.PayloadDigest.bAvailable ||
        !TryComputeRHISha256(Payload.Bytes, ComputedDigest) ||
        ComputedDigest != Payload.PayloadDigest)
    {
        return false;
    }
    return Payload.Format != ERHIShaderPayloadFormat::SPIRV ||
        Payload.Bytes.size() % sizeof(Stoner::Core::uint32) == 0;
}

[[nodiscard]] inline bool IsValidRHIShaderBytecode(const FRHIShaderPayloadDesc& Bytecode) noexcept
{
    constexpr Stoner::Core::uint32 SpirvMagic = 0x07230203u;
    constexpr Stoner::Core::uint32 OpEntryPoint = 15u;
    const std::size_t WordSize = Bytecode.Bytes.size() / sizeof(Stoner::Core::uint32);
    if (!IsValidRHIShaderPayload(Bytecode) ||
        Bytecode.Format != ERHIShaderPayloadFormat::SPIRV ||
        WordSize < 5 || ReadRHIShaderSpirvWord(Bytecode, 0) != SpirvMagic)
    {
        return false;
    }

    const Stoner::Core::uint32 Version = ReadRHIShaderSpirvWord(Bytecode, 1);
    const Stoner::Core::uint32 Major = (Version >> 16u) & 0xffu;
    const Stoner::Core::uint32 Minor = (Version >> 8u) & 0xffu;
    if (Major != 1u || Minor > 6u || (Version & 0xffu) != 0u ||
        ReadRHIShaderSpirvWord(Bytecode, 3) == 0u ||
        ReadRHIShaderSpirvWord(Bytecode, 4) != 0u)
    {
        return false;
    }

    bool bHasEntryPoint = false;
    for (std::size_t WordIndex = 5; WordIndex < WordSize;)
    {
        const Stoner::Core::uint32 Instruction = ReadRHIShaderSpirvWord(Bytecode, WordIndex);
        const std::size_t WordCount = static_cast<std::size_t>(Instruction >> 16u);
        const Stoner::Core::uint32 Opcode = Instruction & 0xffffu;
        if (WordCount == 0 || WordCount > WordSize - WordIndex)
        {
            return false;
        }
        if (Opcode == OpEntryPoint)
        {
            if (WordCount < 4 || ReadRHIShaderSpirvWord(Bytecode, WordIndex + 2U) == 0u ||
                ReadRHIShaderSpirvWord(Bytecode, WordIndex + 2U) >=
                    ReadRHIShaderSpirvWord(Bytecode, 3U))
            {
                return false;
            }
            bool bTerminated = false;
            for (std::size_t NameWord = WordIndex + 3;
                 NameWord < WordIndex + WordCount && !bTerminated;
                 ++NameWord)
            {
                const Stoner::Core::uint32 Packed = ReadRHIShaderSpirvWord(Bytecode, NameWord);
                for (Stoner::Core::uint32 ByteIndex = 0; ByteIndex < 4u; ++ByteIndex)
                {
                    if (((Packed >> (ByteIndex * 8u)) & 0xffu) == 0u)
                    {
                        bTerminated = true;
                        break;
                    }
                }
            }
            if (!bTerminated)
            {
                return false;
            }
            bHasEntryPoint = true;
        }
        WordIndex += WordCount;
    }
    return bHasEntryPoint;
}

[[nodiscard]] constexpr Stoner::Core::uint32
GetRHIShaderExecutionModel(ERHIShaderStage Stage) noexcept
{
    switch (Stage)
    {
    case ERHIShaderStage::Vertex:
        return 0u;
    case ERHIShaderStage::Fragment:
        return 4u;
    case ERHIShaderStage::Compute:
        return 5u;
    default:
        return std::numeric_limits<Stoner::Core::uint32>::max();
    }
}

[[nodiscard]] inline bool DoesRHIShaderBytecodeDeclareEntryPoint(
    const FRHIShaderPayloadDesc& Bytecode,
    ERHIShaderStage Stage,
    const Stoner::Core::FString& EntryPoint) noexcept
{
    constexpr Stoner::Core::uint32 OpEntryPoint = 15u;
    const Stoner::Core::uint32 ExpectedModel = GetRHIShaderExecutionModel(Stage);
    if (!IsValidRHIShaderBytecode(Bytecode) || EntryPoint.IsEmpty() ||
        ExpectedModel == std::numeric_limits<Stoner::Core::uint32>::max())
    {
        return false;
    }

    const std::size_t WordSize = Bytecode.Bytes.size() / sizeof(Stoner::Core::uint32);
    for (std::size_t WordIndex = 5; WordIndex < WordSize;)
    {
        const Stoner::Core::uint32 Instruction = ReadRHIShaderSpirvWord(Bytecode, WordIndex);
        const std::size_t WordCount = static_cast<std::size_t>(Instruction >> 16u);
        const Stoner::Core::uint32 Opcode = Instruction & 0xffffu;
        if (Opcode == OpEntryPoint &&
            ReadRHIShaderSpirvWord(Bytecode, WordIndex + 1U) == ExpectedModel)
        {
            std::size_t CharacterIndex = 0;
            bool bTerminated = false;
            bool bMatches = true;
            for (std::size_t NameWord = WordIndex + 3;
                 NameWord < WordIndex + WordCount && !bTerminated;
                 ++NameWord)
            {
                const Stoner::Core::uint32 Packed = ReadRHIShaderSpirvWord(Bytecode, NameWord);
                for (Stoner::Core::uint32 ByteIndex = 0; ByteIndex < 4u; ++ByteIndex)
                {
                    const char Character = static_cast<char>(
                        (Packed >> (ByteIndex * 8u)) & 0xffu);
                    if (Character == '\0')
                    {
                        bTerminated = true;
                        break;
                    }
                    if (CharacterIndex >= EntryPoint.Len() ||
                        EntryPoint.View()[CharacterIndex] != Character)
                    {
                        bMatches = false;
                    }
                    ++CharacterIndex;
                }
            }
            if (bTerminated && bMatches && CharacterIndex == EntryPoint.Len())
            {
                return true;
            }
        }
        WordIndex += WordCount;
    }
    return false;
}

[[nodiscard]] inline bool IsValidRHIShaderInterfaceBinding(const FRHIShaderInterfaceBinding& Binding) noexcept
{
    return IsValidRHIDescriptorType(Binding.DescriptorType) &&
        Binding.ArrayCount > 0 &&
        IsValidRHIShaderStageFlags(Binding.Visibility);
}

[[nodiscard]] inline bool IsValidRHIShaderConstantRange(const FRHIShaderConstantRange& Range) noexcept
{
    return Range.SizeBytes > 0 &&
        Range.SizeBytes <= std::numeric_limits<Stoner::Core::uint32>::max() - Range.OffsetBytes &&
        IsValidRHIShaderStageFlags(Range.Visibility);
}

[[nodiscard]] inline bool DoRHIShaderConstantRangesOverlap(
    const FRHIShaderConstantRange& Left,
    const FRHIShaderConstantRange& Right) noexcept
{
    const Stoner::Core::uint64 LeftEnd =
        static_cast<Stoner::Core::uint64>(Left.OffsetBytes) + Left.SizeBytes;
    const Stoner::Core::uint64 RightEnd =
        static_cast<Stoner::Core::uint64>(Right.OffsetBytes) + Right.SizeBytes;
    return Left.OffsetBytes < RightEnd && Right.OffsetBytes < LeftEnd;
}

[[nodiscard]] inline bool HasIncompatibleRHIShaderConstantRangeOverlap(
    const Stoner::Core::TArray<FRHIShaderConstantRange>& Ranges) noexcept
{
    for (std::size_t LeftIndex = 0; LeftIndex < Ranges.size(); ++LeftIndex)
    {
        for (std::size_t RightIndex = LeftIndex + 1; RightIndex < Ranges.size(); ++RightIndex)
        {
            if (DoRHIShaderConstantRangesOverlap(Ranges[LeftIndex], Ranges[RightIndex]) &&
                (Ranges[LeftIndex].Visibility & Ranges[RightIndex].Visibility) != ERHIShaderStageFlags::None)
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] inline bool HasDuplicateRHIShaderInterfaceBinding(const FRHIShaderInterfaceMetadata& Metadata) noexcept
{
    for (std::size_t LeftIndex = 0; LeftIndex < Metadata.Bindings.size(); ++LeftIndex)
    {
        for (std::size_t RightIndex = LeftIndex + 1; RightIndex < Metadata.Bindings.size(); ++RightIndex)
        {
            if (Metadata.Bindings[LeftIndex].SetIndex == Metadata.Bindings[RightIndex].SetIndex &&
                Metadata.Bindings[LeftIndex].BindingSlot == Metadata.Bindings[RightIndex].BindingSlot)
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] inline bool IsValidRHIShaderInterfaceMetadata(const FRHIShaderInterfaceMetadata& Metadata, ERHIShaderStage Stage) noexcept
{
    const ERHIShaderStageFlags StageFlag = ToShaderStageFlag(Stage);
    if (StageFlag == ERHIShaderStageFlags::None ||
        HasDuplicateRHIShaderInterfaceBinding(Metadata) ||
        HasIncompatibleRHIShaderConstantRangeOverlap(Metadata.ConstantRanges))
    {
        return false;
    }
    for (const FRHIShaderInterfaceBinding& Binding : Metadata.Bindings)
    {
        if (!IsValidRHIShaderInterfaceBinding(Binding) || !HasRHIFlag(Binding.Visibility, StageFlag))
        {
            return false;
        }
    }
    for (const FRHIShaderConstantRange& Range : Metadata.ConstantRanges)
    {
        if (!IsValidRHIShaderConstantRange(Range) || !HasRHIFlag(Range.Visibility, StageFlag))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool IsValidRHIShaderModuleDesc(const FRHIShaderModuleDesc& Desc) noexcept
{
    if (!IsSupportedRHIShaderStage(Desc.Stage) || Desc.EntryPoint.IsEmpty() ||
        !IsValidRHIShaderPayload(Desc.Payload) ||
        !IsValidRHIShaderInterfaceMetadata(Desc.InterfaceMetadata, Desc.Stage))
    {
        return false;
    }
    if (!Desc.NativeBindingMap.PolicyVersion.IsEmpty() &&
        !IsCanonicalRHINativeBindingMap(Desc.NativeBindingMap))
    {
        return false;
    }
    return Desc.Payload.Format == ERHIShaderPayloadFormat::MetalLibrary ||
        (IsValidRHIShaderBytecode(Desc.Payload) &&
         DoesRHIShaderBytecodeDeclareEntryPoint(
             Desc.Payload, Desc.Stage, Desc.EntryPoint));
}

} // namespace Stoner::RHI
