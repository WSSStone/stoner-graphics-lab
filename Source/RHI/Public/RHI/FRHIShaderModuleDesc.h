#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIDescriptorType.h"
#include "RHI/ERHIShaderStage.h"

#include <cstddef>

namespace Stoner::RHI
{

enum class ERHIShaderBytecodeValidationMode
{
    StructuralFallback,
    Runtime
};

enum class ERHIRuntimeObjectMode
{
    Unknown,
    RealRuntime,
    DeterministicFallback
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
    if (Bytecode.Words.size() < 4 || Bytecode.Format.IsEmpty())
    {
        return false;
    }
    return Bytecode.Format == Stoner::Core::FString("SPIR-V") && Bytecode.Words[0] == 0x07230203u;
}

[[nodiscard]] inline bool IsValidRHIShaderInterfaceBinding(const FRHIShaderInterfaceBinding& Binding) noexcept
{
    return Binding.ArrayCount > 0 && Binding.Visibility != ERHIShaderStageFlags::None;
}

[[nodiscard]] inline bool IsValidRHIShaderConstantRange(const FRHIShaderConstantRange& Range) noexcept
{
    return Range.SizeBytes > 0 && Range.Visibility != ERHIShaderStageFlags::None;
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
    if (StageFlag == ERHIShaderStageFlags::None || HasDuplicateRHIShaderInterfaceBinding(Metadata))
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
        IsValidRHIShaderBytecode(Desc.Bytecode) && IsValidRHIShaderInterfaceMetadata(Desc.InterfaceMetadata, Desc.Stage);
}

} // namespace Stoner::RHI
