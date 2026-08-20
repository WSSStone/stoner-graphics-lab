#pragma once

#include "RHI/FRHIShaderModuleDesc.h"

#include <string_view>

namespace Stoner::Tests
{

[[nodiscard]] inline Stoner::Core::TArray<Stoner::Core::uint32>
MakeMinimalShaderBytecode(
    Stoner::RHI::ERHIShaderStage Stage,
    std::string_view EntryPoint)
{
    using Stoner::Core::uint32;
    using Stoner::RHI::GetRHIShaderExecutionModel;

    constexpr uint32 OpCapability = 17u;
    constexpr uint32 OpMemoryModel = 14u;
    constexpr uint32 OpEntryPoint = 15u;
    constexpr uint32 OpExecutionMode = 16u;
    constexpr uint32 OpTypeVoid = 19u;
    constexpr uint32 OpTypeFunction = 33u;
    constexpr uint32 OpFunction = 54u;
    constexpr uint32 OpFunctionEnd = 56u;
    constexpr uint32 OpLabel = 248u;
    constexpr uint32 OpReturn = 253u;

    Stoner::Core::TArray<uint32> Words = {
        0x07230203u,
        0x00010000u,
        0u,
        5u,
        0u,
        (2u << 16u) | OpCapability,
        1u,
        (3u << 16u) | OpMemoryModel,
        0u,
        1u,
    };

    const std::size_t NameWordCount = (EntryPoint.size() + 1u + 3u) / 4u;
    Words.push_back(
        (static_cast<uint32>(3u + NameWordCount) << 16u) | OpEntryPoint);
    Words.push_back(GetRHIShaderExecutionModel(Stage));
    Words.push_back(1u);
    for (std::size_t NameWord = 0; NameWord < NameWordCount; ++NameWord)
    {
        uint32 Packed = 0u;
        for (std::size_t ByteIndex = 0; ByteIndex < 4u; ++ByteIndex)
        {
            const std::size_t CharacterIndex = NameWord * 4u + ByteIndex;
            if (CharacterIndex < EntryPoint.size())
            {
                Packed |= static_cast<uint32>(
                    static_cast<unsigned char>(EntryPoint[CharacterIndex]))
                    << static_cast<uint32>(ByteIndex * 8u);
            }
        }
        Words.push_back(Packed);
    }

    if (Stage == Stoner::RHI::ERHIShaderStage::Fragment)
    {
        Words.push_back((3u << 16u) | OpExecutionMode);
        Words.push_back(1u);
        Words.push_back(7u);
    }
    else if (Stage == Stoner::RHI::ERHIShaderStage::Compute)
    {
        Words.push_back((6u << 16u) | OpExecutionMode);
        Words.push_back(1u);
        Words.push_back(17u);
        Words.push_back(1u);
        Words.push_back(1u);
        Words.push_back(1u);
    }

    const Stoner::Core::TArray<uint32> FunctionWords = {
        (2u << 16u) | OpTypeVoid,
        2u,
        (3u << 16u) | OpTypeFunction,
        3u,
        2u,
        (5u << 16u) | OpFunction,
        2u,
        1u,
        0u,
        3u,
        (2u << 16u) | OpLabel,
        4u,
        (1u << 16u) | OpReturn,
        (1u << 16u) | OpFunctionEnd,
    };
    Words.insert(Words.end(), FunctionWords.begin(), FunctionWords.end());
    return Words;
}

[[nodiscard]] inline Stoner::RHI::FRHIShaderPayloadDesc
MakeMinimalShaderPayload(
    Stoner::RHI::ERHIShaderStage Stage,
    std::string_view EntryPoint,
    const Stoner::Core::FString& Identity = "test-shader",
    const Stoner::Core::FString& TargetProfile = "legacy-vulkan-v1")
{
    Stoner::RHI::FRHIShaderPayloadDesc Payload;
    const auto Words = MakeMinimalShaderBytecode(Stage, EntryPoint);
    (void)Stoner::RHI::SetRHIShaderSpirvWords(
        Payload, Words, Identity, TargetProfile);
    return Payload;
}

} // namespace Stoner::Tests
