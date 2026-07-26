# B02-S01: Core Value Identity And Containers Inspection

## Inspection Budget

The inspection covered one responsibility domain and eight production headers,
totaling 320 lines:

1. `Source/Core/Public/Core/FPlatformTypes.h`
2. `Source/Core/Public/Core/FString.h`
3. `Source/Core/Public/Core/FName.h`
4. `Source/Core/Public/Core/TArray.h`
5. `Source/Core/Public/Core/TMap.h`
6. `Source/Core/Public/Core/TSharedPtr.h`
7. `Source/Core/Public/Core/TUniquePtr.h`
8. `Source/Core/Public/Core/CoreMinimal.h`

Supporting evidence included Feature 003's spec, plan, research, data model,
public API contract, tasks, `Tests/CoreFoundationTests.cpp`, Git history, and
repository-wide call-site searches. No production implementation was changed.

## Requirement Mapping

- `003-FR-001`: fixed-width, size, pointer, character, and boolean vocabulary
  is present in `FPlatformTypes`.
- `003-FR-002`: `FString` provides the required owning value operations; its
  move test does not establish a meaningful moved-from postcondition.
- `003-FR-003`: `FName` stores text and hash, but two reachable states violate
  its text-derived identity and equality invariant.
- `003-FR-004`: standard-library-backed shared and unique ownership aliases and
  construction helpers are present.
- `003-FR-006`: array and map aliases provide the contracted initial standard
  collection semantics.
- `003-FR-007` and `003-SC-006`: the inspected public headers depend only on
  Core headers and the C++ standard library.
- `003-FR-008`: normal cases exist, but moved-from boundary coverage is
  mechanically ineffective and omits `FName`.

## Reproduction

A standalone C++20 probe constructed and moved an `FName`, then compared the
moved-from value with the default empty name. It also used the public synthetic
hash factory to create two names with the same text and different hashes.

Observed output:

```text
1 0 0
```

The three fields mean:

1. The moved-from name reports that it is empty.
2. That name does not compare equal to the default empty name.
3. Identical text with different caller-supplied hashes compares unequal.

The first result occurs because implicit memberwise move transfers the string
but leaves the old hash in the source. The third result occurs because a public
test factory permits callers to break the same text/hash invariant.

## Findings

### CR001-B02-F001 - Accepted S2

`FName` has public or language-generated states in which text and hash disagree.
Equality rejects equal text when hashes differ, contrary to `003-FR-003` and
the Engine Name validation rules.

### CR001-B02-F002 - Accepted S2

The moved-from `FString` assertion is `IsEmpty() || !IsEmpty()`, which is true
for every Boolean result. `FName` copy and move laws are not tested, allowing
CR001-B02-F001 to pass the complete test matrix.

## Confirmed Strengths

- `FString` owns its data and safely normalizes a null C-string input to empty.
- `FName` compares text after matching hashes, so the tested same-hash,
  different-text collision case is not hash-only.
- Pointer and container aliases intentionally retain standard semantics, as
  selected by Feature 003 research.
- The foundation headers introduce no upward engine-layer dependency.
- Existing downstream use is broad for strings, arrays, and shared ownership,
  but repository use of `FName` is currently limited, reducing the immediate
  blast radius of the identity repair.

## Non-Findings And Limits

- `std::hash<std::string_view>` is not guaranteed to be a persistent identifier,
  but Feature 003 explicitly requires only process-local hash stability.
- Lack of `TMap<FName, V>` integration is not a finding because Feature 003
  contracts the name and map vocabularies independently and no current caller
  requires that combination.
- `CoreMinimal.h` has accumulated later Core feature headers. This does not show
  that Feature 003 implemented excluded features, and include-cost analysis is
  reserved for the cross-cutting batch.

## B02-S02 Fix Packet

The next step may repair both related findings:

1. Preserve the `FName` text/hash invariant across move construction and move
   assignment, and prevent the public collision-test mechanism from creating
   text-equality contradictions.
2. Replace tautological coverage with explicit copy/move/equality law tests,
   including destination and moved-from source behavior.

