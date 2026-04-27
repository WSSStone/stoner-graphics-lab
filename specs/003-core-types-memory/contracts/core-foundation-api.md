# Contract: Core Foundation Public API

**Feature**: 003-core-types-memory  
**Date**: 2026-04-27

## Overview

This contract defines the public Core foundation surface that later engine layers may depend on. The exact implementation may evolve, but the names and observable behavior below must remain stable for this feature.

## Public Header Contract

All headers live under `Source/Core/Public/Core/` and are included as `Core/<Header>.h`.

| Header | Public Deliverable | Required Behavior |
|--------|--------------------|-------------------|
| `FPlatformTypes.h` | `FPlatformTypes` and fixed-width aliases | Exposes documented signed, unsigned, character, size, and pointer-sized type vocabulary |
| `FString.h` | `FString` | Owning text value with default, text, copy, move, comparison, length, empty, and diagnostic view behavior |
| `FName.h` | `FName` | Immutable text-derived identifier with hash fast path and collision-safe equality |
| `TSharedPtr.h` | `TSharedPtr<T>` | Shared ownership pointer vocabulary |
| `TUniquePtr.h` | `TUniquePtr<T>` | Unique ownership pointer vocabulary |
| `FMemory.h` | `FMemory` | Allocation, aligned allocation, deallocation, copy, move, set, and zero memory operations |
| `TArray.h` | `TArray<T>` | Dynamic array vocabulary |
| `TMap.h` | `TMap<K, V>` | Key-value map vocabulary |
| `CoreMinimal.h` | Core public aggregate | Includes the stable Core foundation headers above |

## Namespace Contract

Public deliverables must be available through the Core namespace used by the project skeleton:

```cpp
namespace Stoner::Core
{
    // Core foundation deliverables live here.
}
```

Global aliases may be introduced only if the implementation plan explicitly chooses them during tasks and they do not conflict with the namespace contract.

## Behavioral Contract

### Platform Types

- Fixed-width signed and unsigned aliases must match their documented sizes.
- Size and pointer-sized aliases must be suitable for sizes, counts, and pointer round-tripping where documented.
- Type definitions must not require higher-layer includes.

### `FString`

- Default construction creates an empty string.
- Construction from text preserves that text.
- Copying preserves text equality.
- Moving leaves the moved-from value valid for destruction, assignment, and emptiness queries.
- Equality compares text content.
- Length and emptiness queries are deterministic.

### `FName`

- Default construction creates an empty name.
- Construction from identical text creates names that compare equal.
- Construction from different text creates names that compare unequal.
- Equality must remain correct even if two names have the same hash.
- Hash values are observable only as an optimization detail unless implementation tasks expose an explicit accessor.

### Ownership Pointers

- `TUniquePtr<T>` communicates exclusive ownership and is movable.
- `TSharedPtr<T>` communicates shared ownership and is copyable.
- Null pointer values are valid for both ownership modes.

### `FMemory`

- `Allocate(size)` returns an owned memory block for positive sizes or a deterministic null result for zero size.
- `AllocateAligned(size, alignment)` returns an owned memory block aligned to `alignment` when `size` and `alignment` are valid.
- Invalid alignment requests fail deterministically.
- `Deallocate` and aligned deallocation accept null.
- `Copy`, `Move`, `Set`, and `Zero` affect exactly the requested byte range.

### Containers

- `TArray<T>` default construction creates an empty dynamic array.
- `TMap<K, V>` default construction creates an empty key-value map.
- Inserted values can be retrieved using the selected collection's normal access pattern.
- Copy and move semantics follow the selected backing collection semantics for this phase.

## Verification Contract

The Core foundation verification suite must cover:

- Fixed-width type size expectations.
- Empty and non-empty `FString` behavior.
- Copy and move `FString` behavior.
- Empty, duplicate, different, and collision-safe `FName` equality behavior.
- Null, move, and shared ownership pointer behavior.
- Allocation, aligned allocation, deallocation, copy, move, set, and zero memory behavior.
- Empty, insert, retrieve, copy, and move behavior for array and map vocabulary.

## Exclusions

- No math types.
- No logging/assertion macros.
- No platform file, process, time, or window APIs.
- No RHI, Backend, Renderer, or Application dependencies.
- No persistent name registry or cross-run stable name IDs.
