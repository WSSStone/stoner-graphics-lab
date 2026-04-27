#pragma once

#include <cstddef>

namespace Stoner::Core
{

class FMemory
{
public:
    [[nodiscard]] static void* Allocate(std::size_t Size) noexcept;
    static void Deallocate(void* Pointer) noexcept;

    [[nodiscard]] static void* AllocateAligned(std::size_t Size, std::size_t Alignment) noexcept;
    static void DeallocateAligned(void* Pointer) noexcept;

    static void Copy(void* Destination, const void* Source, std::size_t Size) noexcept;
    static void Move(void* Destination, const void* Source, std::size_t Size) noexcept;
    static void Set(void* Destination, int Value, std::size_t Size) noexcept;
    static void Zero(void* Destination, std::size_t Size) noexcept;
};

} // namespace Stoner::Core
