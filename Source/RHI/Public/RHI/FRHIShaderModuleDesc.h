#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIDescriptorType.h"
#include "RHI/ERHIShaderStage.h"
#include "RHI/ERHIRuntimeMode.h"

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

struct FRHIShaderBytecodeDesc
{
    Stoner::Core::TArray<Stoner::Core::uint32> Words;
    Stoner::Core::FString Format = "SPIR-V";
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
    Stoner::Core::FString PayloadIdentity;
    FRHIShaderBytecodeDesc Bytecode;
    FRHIShaderInterfaceMetadata InterfaceMetadata;
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

[[nodiscard]] inline bool IsValidRHIShaderBytecode(const FRHIShaderBytecodeDesc& Bytecode) noexcept
{
    constexpr Stoner::Core::uint32 SpirvMagic = 0x07230203u;
    constexpr Stoner::Core::uint32 OpEntryPoint = 15u;
    if (Bytecode.Words.size() < 5 ||
        Bytecode.Format.View() != "SPIR-V" ||
        Bytecode.Words[0] != SpirvMagic)
    {
        return false;
    }

    const Stoner::Core::uint32 Version = Bytecode.Words[1];
    const Stoner::Core::uint32 Major = (Version >> 16u) & 0xffu;
    const Stoner::Core::uint32 Minor = (Version >> 8u) & 0xffu;
    if (Major != 1u || Minor > 6u || (Version & 0xffu) != 0u ||
        Bytecode.Words[3] == 0u || Bytecode.Words[4] != 0u)
    {
        return false;
    }

    bool bHasEntryPoint = false;
    for (std::size_t WordIndex = 5; WordIndex < Bytecode.Words.size();)
    {
        const Stoner::Core::uint32 Instruction = Bytecode.Words[WordIndex];
        const std::size_t WordCount = static_cast<std::size_t>(Instruction >> 16u);
        const Stoner::Core::uint32 Opcode = Instruction & 0xffffu;
        if (WordCount == 0 || WordCount > Bytecode.Words.size() - WordIndex)
        {
            return false;
        }
        if (Opcode == OpEntryPoint)
        {
            if (WordCount < 4 || Bytecode.Words[WordIndex + 2] == 0u ||
                Bytecode.Words[WordIndex + 2] >= Bytecode.Words[3])
            {
                return false;
            }
            bool bTerminated = false;
            for (std::size_t NameWord = WordIndex + 3;
                 NameWord < WordIndex + WordCount && !bTerminated;
                 ++NameWord)
            {
                const Stoner::Core::uint32 Packed = Bytecode.Words[NameWord];
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
    const FRHIShaderBytecodeDesc& Bytecode,
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

    for (std::size_t WordIndex = 5; WordIndex < Bytecode.Words.size();)
    {
        const Stoner::Core::uint32 Instruction = Bytecode.Words[WordIndex];
        const std::size_t WordCount = static_cast<std::size_t>(Instruction >> 16u);
        const Stoner::Core::uint32 Opcode = Instruction & 0xffffu;
        if (Opcode == OpEntryPoint && Bytecode.Words[WordIndex + 1] == ExpectedModel)
        {
            std::size_t CharacterIndex = 0;
            bool bTerminated = false;
            bool bMatches = true;
            for (std::size_t NameWord = WordIndex + 3;
                 NameWord < WordIndex + WordCount && !bTerminated;
                 ++NameWord)
            {
                const Stoner::Core::uint32 Packed = Bytecode.Words[NameWord];
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
    return IsSupportedRHIShaderStage(Desc.Stage) && !Desc.EntryPoint.IsEmpty() && !Desc.PayloadIdentity.IsEmpty() &&
        IsValidRHIShaderBytecode(Desc.Bytecode) &&
        DoesRHIShaderBytecodeDeclareEntryPoint(Desc.Bytecode, Desc.Stage, Desc.EntryPoint) &&
        IsValidRHIShaderInterfaceMetadata(Desc.InterfaceMetadata, Desc.Stage);
}

} // namespace Stoner::RHI
