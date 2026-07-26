# B02-S05: Core Memory And Module Lifecycle Fix

## Scope

This fix resolves the single accepted finding from B02-S04:

- `CR001-B02-F003` (S1): aligned-allocation size arithmetic can wrap into an
  undersized successful allocation.

No module lifecycle API was added or changed. `CoreInit` remains the
Feature 001 skeleton placeholder documented by the preceding inspection.

## Implementation

Commit `60689e1`:

1. Computes aligned-allocation header and padding overhead separately.
2. Rejects requests greater than
   `SIZE_MAX - sizeof(void*) - (Alignment - 1)` before any addition or
   allocation.
3. Reuses the checked padding value for address alignment.
4. Documents zero-size/failure behavior, accepted alignment shape, and the
   required deallocation pairing in `FMemory.h`.
5. Adds regression coverage for the first size beyond the representable
   boundary and for `SIZE_MAX`.

## Local Verification

- `git diff --check`: passed.
- Strict Debug fallback build with `-Werror`: passed.
- Full strict Debug deterministic test executable: passed.
- Core foundation result: `62 passed, 0 failed`.
- Strict Release build with `-Werror`: passed.
- ASan/UBSan strict Debug build and full test executable: passed.
- Original focused probe:
  - Before: `non-null`, exit `1`.
  - After: `null`, exit `0`.

The authoritative gate records are:

- `Evidence/gate-strict-release.json`
- `Evidence/gate-sanitizers.json`

## Finding State

`CR001-B02-F003` is Fixed at `60689e1`. Cross-platform hosted verification is
reserved for the next verification packet; the finding is not yet marked
Verified.

