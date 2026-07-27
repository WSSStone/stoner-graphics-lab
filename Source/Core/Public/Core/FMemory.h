#pragma once

#include <cstddef>

namespace Stoner::Core
{

class FMemory
{
public:
    // Allocate returns nullptr for zero size or allocation failure. Release with Deallocate.
    [[nodiscard]] static void* Allocate(std::size_t Size) noexcept;
    static void Deallocate(void* Pointer) noexcept;

    // Alignment must be a power of two and at least sizeof(void*). Invalid,
    // unrepresentable, or failed requests return nullptr. Release only with DeallocateAligned.
    [[nodiscard]] static void* AllocateAligned(std::size_t Size, std::size_t Alignment) noexcept;
    static void DeallocateAligned(void* Pointer) noexcept;

    static void Copy(void* Destination, const void* Source, std::size_t Size) noexcept;
    static void Move(void* Destination, const void* Source, std::size_t Size) noexcept;
    static void Set(void* Destination, int Value, std::size_t Size) noexcept;
    static void Zero(void* Destination, std::size_t Size) noexcept;
};

} // namespace Stoner::Core
