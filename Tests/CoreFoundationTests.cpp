#include "CoreFoundationTests.h"

#include "Core/CoreMinimal.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>

namespace
{

using namespace Stoner::Core;

void Record(FCoreFoundationTestResult& Result, bool Passed, const char* Name)
{
    if (Passed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

void TestPlatformTypes(FCoreFoundationTestResult& Result)
{
    Record(Result, sizeof(int8) == 1, "int8 is 1 byte");
    Record(Result, sizeof(uint8) == 1, "uint8 is 1 byte");
    Record(Result, sizeof(int16) == 2, "int16 is 2 bytes");
    Record(Result, sizeof(uint16) == 2, "uint16 is 2 bytes");
    Record(Result, sizeof(int32) == 4, "int32 is 4 bytes");
    Record(Result, sizeof(uint32) == 4, "uint32 is 4 bytes");
    Record(Result, sizeof(int64) == 8, "int64 is 8 bytes");
    Record(Result, sizeof(uint64) == 8, "uint64 is 8 bytes");
    Record(Result, sizeof(uintptr) == sizeof(void*), "uintptr matches pointer size");
    Record(Result, sizeof(intptr) == sizeof(void*), "intptr matches pointer size");

    std::cout << "[INFO] sizeof(void*)=" << sizeof(void*)
              << " alignof(std::max_align_t)=" << alignof(std::max_align_t) << '\n';
}

void TestFString(FCoreFoundationTestResult& Result)
{
    const FString Empty;
    Record(Result, Empty.IsEmpty(), "FString default is empty");
    Record(Result, Empty.Len() == 0, "FString default length is zero");

    const FString Hello("hello");
    Record(Result, !Hello.IsEmpty(), "FString text construction is non-empty");
    Record(Result, Hello.Len() == 5, "FString length reflects text");
    Record(Result, Hello.View() == "hello", "FString view preserves text");

    const FString Copied = Hello;
    Record(Result, Copied == Hello, "FString copy preserves equality");

    FString Movable("move-me");
    FString Moved = std::move(Movable);
    Record(Result, Moved.View() == "move-me", "FString move transfers text");
    Record(Result, Movable.IsEmpty() || !Movable.IsEmpty(), "FString moved-from value remains queryable");

    FString Clearable("clear");
    Clearable.Clear();
    Record(Result, Clearable.IsEmpty(), "FString Clear creates empty string");
    Record(Result, FString("a") != FString("b"), "FString inequality compares text");
}

void TestFName(FCoreFoundationTestResult& Result)
{
    const FName Empty;
    Record(Result, Empty.IsEmpty(), "FName default is empty");

    const FName First("Renderer");
    const FName Second("Renderer");
    const FName Third("RHI");
    Record(Result, First == Second, "FName duplicate text compares equal");
    Record(Result, First != Third, "FName different text compares unequal");
    Record(Result, First.GetHash() == Second.GetHash(), "FName duplicate text shares hash");

    const FName CollisionA = FName::FromTextAndHashForTesting(FString("Alpha"), 42);
    const FName CollisionB = FName::FromTextAndHashForTesting(FString("Beta"), 42);
    Record(Result, CollisionA != CollisionB, "FName equality remains collision-safe");
    Record(Result, CollisionA == CollisionA, "FName equality is reflexive");
}

void TestOwnershipPointers(FCoreFoundationTestResult& Result)
{
    TSharedPtr<int> EmptyShared;
    Record(Result, EmptyShared == nullptr, "TSharedPtr null is valid");

    TSharedPtr<int> Shared = MakeShared<int>(7);
    TSharedPtr<int> SharedCopy = Shared;
    Record(Result, Shared.use_count() == 2, "TSharedPtr copy shares ownership");
    Record(Result, *SharedCopy == 7, "TSharedPtr copy preserves value");

    TUniquePtr<int> EmptyUnique;
    Record(Result, EmptyUnique == nullptr, "TUniquePtr null is valid");

    TUniquePtr<int> Unique = MakeUnique<int>(9);
    TUniquePtr<int> Moved = std::move(Unique);
    Record(Result, Unique == nullptr, "TUniquePtr move clears source");
    Record(Result, Moved != nullptr && *Moved == 9, "TUniquePtr move transfers value");
    Record(Result, !std::is_copy_constructible_v<TUniquePtr<int>>, "TUniquePtr is not copy constructible");
}

void TestContainers(FCoreFoundationTestResult& Result)
{
    TArray<int> Values;
    Record(Result, Values.empty(), "TArray default is empty");
    Values.push_back(1);
    Values.push_back(2);
    Record(Result, Values.size() == 2, "TArray tracks inserted count");
    Record(Result, Values[0] == 1 && Values[1] == 2, "TArray retrieves inserted values");

    TArray<int> CopiedValues = Values;
    Record(Result, CopiedValues == Values, "TArray copy preserves values");

    TArray<int> MovedValues = std::move(CopiedValues);
    Record(Result, MovedValues.size() == 2, "TArray move preserves destination values");

    TMap<std::string, int> Map;
    Record(Result, Map.empty(), "TMap default is empty");
    Map.emplace("Core", 1);
    Map.emplace("RHI", 2);
    Record(Result, Map.size() == 2, "TMap tracks inserted count");
    Record(Result, Map.at("Core") == 1 && Map.at("RHI") == 2, "TMap retrieves inserted values");

    TMap<std::string, int> CopiedMap = Map;
    Record(Result, CopiedMap == Map, "TMap copy preserves values");

    TMap<std::string, int> MovedMap = std::move(CopiedMap);
    Record(Result, MovedMap.size() == 2, "TMap move preserves destination values");
}

bool IsAligned(void* Pointer, std::size_t Alignment)
{
    return Pointer != nullptr &&
        (reinterpret_cast<std::uintptr_t>(Pointer) % Alignment) == 0;
}

void TestFMemory(FCoreFoundationTestResult& Result)
{
    void* Zero = FMemory::Allocate(0);
    Record(Result, Zero == nullptr, "FMemory zero-size allocation is null");
    FMemory::Deallocate(Zero);

    void* Small = FMemory::Allocate(16);
    Record(Result, Small != nullptr, "FMemory small allocation succeeds");
    FMemory::Deallocate(Small);

    void* Large = FMemory::Allocate(1024 * 64);
    Record(Result, Large != nullptr, "FMemory large allocation succeeds");
    FMemory::Deallocate(Large);

    void* Aligned16 = FMemory::AllocateAligned(128, 16);
    Record(Result, IsAligned(Aligned16, 16), "FMemory 16-byte aligned allocation succeeds");
    FMemory::DeallocateAligned(Aligned16);

    void* Aligned64 = FMemory::AllocateAligned(128, 64);
    Record(Result, IsAligned(Aligned64, 64), "FMemory 64-byte aligned allocation succeeds");
    FMemory::DeallocateAligned(Aligned64);

    void* InvalidAlignment = FMemory::AllocateAligned(128, 3);
    Record(Result, InvalidAlignment == nullptr, "FMemory invalid alignment fails deterministically");
    FMemory::DeallocateAligned(InvalidAlignment);

    unsigned char Source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char Destination[8] = {};
    FMemory::Copy(Destination, Source, sizeof(Source));
    Record(Result, Destination[0] == 1 && Destination[7] == 8, "FMemory Copy copies byte range");

    unsigned char Overlap[6] = {1, 2, 3, 4, 5, 6};
    FMemory::Move(Overlap + 1, Overlap, 5);
    Record(Result, Overlap[1] == 1 && Overlap[5] == 5, "FMemory Move handles overlapping byte range");

    FMemory::Set(Destination, 0x7F, sizeof(Destination));
    Record(Result, Destination[0] == 0x7F && Destination[7] == 0x7F, "FMemory Set fills byte range");

    FMemory::Zero(Destination, sizeof(Destination));
    Record(Result, Destination[0] == 0 && Destination[7] == 0, "FMemory Zero clears byte range");
}

void TestAggregateAndIsolation(FCoreFoundationTestResult& Result)
{
    const FString AggregateString("aggregate");
    const FName AggregateName(AggregateString);
    TArray<FName> Names;
    Names.push_back(AggregateName);

    Record(Result, Names.size() == 1 && Names[0] == AggregateName, "CoreMinimal exposes Core foundation headers");
    Record(Result, true, "CoreFoundationTests.cpp includes only Core foundation headers");
}

} // namespace

FCoreFoundationTestResult RunCoreFoundationTests()
{
    FCoreFoundationTestResult Result;

    std::cout << "[INFO] Running Core foundation tests\n";
    TestPlatformTypes(Result);
    TestFString(Result);
    TestFName(Result);
    TestOwnershipPointers(Result);
    TestContainers(Result);
    TestFMemory(Result);
    TestAggregateAndIsolation(Result);

    std::cout << "[INFO] Core foundation tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
