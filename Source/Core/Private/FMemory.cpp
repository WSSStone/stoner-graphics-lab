#include "Core/FMemory.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace Stoner::Core
{

namespace
{

[[nodiscard]] bool IsPowerOfTwo(std::size_t Value) noexcept
{
    return Value != 0 && (Value & (Value - 1)) == 0;
}

} // namespace

void* FMemory::Allocate(std::size_t Size) noexcept
{
    if (Size == 0)
    {
        return nullptr;
    }

    return std::malloc(Size);
}

void FMemory::Deallocate(void* Pointer) noexcept
{
    std::free(Pointer);
}

void* FMemory::AllocateAligned(std::size_t Size, std::size_t Alignment) noexcept
{
    if (Size == 0 || !IsPowerOfTwo(Alignment) || Alignment < sizeof(void*))
    {
        return nullptr;
    }

    const std::size_t HeaderSize = sizeof(void*);
    const std::size_t PaddingSize = Alignment - 1;
    if (Size > std::numeric_limits<std::size_t>::max() - HeaderSize - PaddingSize)
    {
        return nullptr;
    }

    const std::size_t TotalSize = Size + PaddingSize + HeaderSize;
    void* Raw = std::malloc(TotalSize);
    if (Raw == nullptr)
    {
        return nullptr;
    }

    const auto RawAddress = reinterpret_cast<std::uintptr_t>(Raw) + HeaderSize;
    const auto AlignedAddress = (RawAddress + PaddingSize) & ~(static_cast<std::uintptr_t>(Alignment) - 1);
    auto* Aligned = reinterpret_cast<void*>(AlignedAddress);

    reinterpret_cast<void**>(Aligned)[-1] = Raw;
    return Aligned;
}

void FMemory::DeallocateAligned(void* Pointer) noexcept
{
    if (Pointer == nullptr)
    {
        return;
    }

    void* Raw = reinterpret_cast<void**>(Pointer)[-1];
    std::free(Raw);
}

void FMemory::Copy(void* Destination, const void* Source, std::size_t Size) noexcept
{
    if (Size == 0 || Destination == nullptr || Source == nullptr)
    {
        return;
    }

    std::memcpy(Destination, Source, Size);
}

void FMemory::Move(void* Destination, const void* Source, std::size_t Size) noexcept
{
    if (Size == 0 || Destination == nullptr || Source == nullptr)
    {
        return;
    }

    std::memmove(Destination, Source, Size);
}

void FMemory::Set(void* Destination, int Value, std::size_t Size) noexcept
{
    if (Size == 0 || Destination == nullptr)
    {
        return;
    }

    std::memset(Destination, Value, Size);
}

void FMemory::Zero(void* Destination, std::size_t Size) noexcept
{
    Set(Destination, 0, Size);
}

} // namespace Stoner::Core
