# B02-S04: Core Memory And Module Lifecycle Inspection

## Inspection Budget

The inspection covered one responsibility domain and three production files,
totaling 132 lines:

1. `Source/Core/Public/Core/FMemory.h`
2. `Source/Core/Private/FMemory.cpp`
3. `Source/Core/Private/CoreModule.cpp`

Supporting evidence included `Source/Core/SConscript`, Feature 003's spec,
research, data model, public API contract and tasks,
`Tests/CoreFoundationTests.cpp`, Git history, and repository-wide call-site
searches. No production implementation was changed.

## Requirement Mapping

- `003-FR-005`: all required allocation, aligned allocation, deallocation and
  byte-operation entry points exist.
- `003-FR-007` and `003-SC-006`: the implementation depends only on Core and
  the C++ standard library.
- `003-FR-008`: normal allocation, zero-size allocation, two common
  alignments, one invalid alignment, overlap-safe move, copy, set and zero are
  covered. Representability boundaries are not covered.
- `003-FR-009`: the feature contract describes ownership and matching
  deallocation at a high level. Exact accepted alignment rules are only
  discoverable from the implementation; this should be clarified with the
  accepted repair but is not a separate safety finding.
- User Story 2 scenario 3 and the Memory Utility validation rules require an
  invalid or unsupported request to fail deterministically without corrupting
  existing memory.

## Reproduction

A standalone C++20 probe requested an aligned allocation with:

```text
size      = SIZE_MAX
alignment = 16
```

The probe did not write through the returned pointer. It only printed whether
the result was null and released a non-null result through the matching API.

Observed output on macOS arm64:

```text
non-null
probe_exit=1
```

`AllocateAligned` computes the backing allocation size using unchecked
`std::size_t` addition. The request wraps to a small representable value before
calling `malloc`, so the API reports success for a block much smaller than the
requested size.

## Finding

### CR001-B02-F003 - Accepted S1

Aligned allocation can return an undersized successful block when alignment
overhead overflows `std::size_t`. A caller that writes the requested byte count
will access beyond the allocation.

## Confirmed Strengths

- Zero-size allocation and null deallocation have explicit deterministic
  behavior.
- Alignment must be a power of two and at least pointer alignment, which keeps
  the hidden backing-pointer slot naturally aligned.
- The byte operations return before invoking the C library when the requested
  size is zero or a required pointer is null.
- `Move` correctly uses `memmove` for overlap; `Copy` uses `memcpy`.
- The aligned allocation's backing pointer is paired with a dedicated
  deallocation path and existing valid 16-byte and 64-byte cases pass.
- `FMemory` currently has no downstream production call sites, reducing the
  immediate blast radius while the contract is repaired.

## Module Lifecycle Assessment

`CoreInit` is an internal, unreferenced placeholder introduced explicitly by
Feature 001's skeleton task. It has no public declaration, owns no state, and
does not claim initialization or shutdown semantics. Treating it as a module
lifecycle API would invent a contract that the roadmap and specs do not
require, so no finding is recorded.

## B02-S05 Fix Packet

The next step should:

1. Reject aligned allocation requests when header and alignment padding cannot
   be added to `Size` without overflowing `std::size_t`.
2. Add exact-boundary and overflowing-size regression tests, preserving
   deterministic null results across Windows, macOS, and Linux.
3. Document the accepted alignment shape and the matching aligned
   deallocation requirement at the public API boundary.

