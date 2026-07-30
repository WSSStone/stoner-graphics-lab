#include "FShaderPayloadValidation.h"

#include <cstring>

namespace Stoner::Asset::Private
{
namespace
{

Core::uint32 ReadWord(const Core::TArray<Core::uint8>& Bytes, std::size_t Index)
{
    Core::uint32 Word = 0;
    std::memcpy(&Word, Bytes.data() + Index * 4, sizeof(Word));
    return Word;
}

Core::uint32 ExecutionModel(EShaderStage Stage)
{
    switch (Stage)
    {
    case EShaderStage::Vertex: return 0;
    case EShaderStage::Fragment: return 4;
    case EShaderStage::Compute: return 5;
    }
    return ~Core::uint32{0};
}

} // namespace

EAssetResult ValidateShaderPayloadBytes(
    const Core::TArray<Core::uint8>& Bytes,
    EShaderPayloadFormat Format,
    EShaderStage Stage,
    const Core::FString& EntryPoint)
{
    if (Format != EShaderPayloadFormat::SPIRV ||
        Bytes.size() < 20 ||
        Bytes.size() % 4 != 0 ||
        EntryPoint.IsEmpty())
    {
        return EAssetResult::DependencyMismatch;
    }
    const std::size_t WordCount = Bytes.size() / 4;
    if (ReadWord(Bytes, 0) != 0x07230203U ||
        (ReadWord(Bytes, 1) >> 16U) != 1U ||
        ((ReadWord(Bytes, 1) >> 8U) & 0xffU) > 6U ||
        ReadWord(Bytes, 3) == 0 ||
        ReadWord(Bytes, 4) != 0)
    {
        return EAssetResult::DependencyMismatch;
    }
    for (std::size_t Index = 5; Index < WordCount;)
    {
        const Core::uint32 Instruction = ReadWord(Bytes, Index);
        const std::size_t Count = Instruction >> 16U;
        const Core::uint32 Opcode = Instruction & 0xffffU;
        if (Count == 0 || Count > WordCount - Index)
        {
            return EAssetResult::DependencyMismatch;
        }
        if (Opcode == 15U && Count >= 4 &&
            ReadWord(Bytes, Index + 1) == ExecutionModel(Stage))
        {
            std::string Name;
            bool bTerminated = false;
            for (std::size_t Word = Index + 3;
                 Word < Index + Count && !bTerminated;
                 ++Word)
            {
                const Core::uint32 Packed = ReadWord(Bytes, Word);
                for (unsigned Byte = 0; Byte < 4; ++Byte)
                {
                    const char Character =
                        static_cast<char>((Packed >> (Byte * 8U)) & 0xffU);
                    if (Character == '\0')
                    {
                        bTerminated = true;
                        break;
                    }
                    Name.push_back(Character);
                }
            }
            if (bTerminated && Name == EntryPoint.View())
            {
                return EAssetResult::Success;
            }
        }
        Index += Count;
    }
    return EAssetResult::DependencyMismatch;
}

} // namespace Stoner::Asset::Private
