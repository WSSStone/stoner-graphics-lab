# B02-S02: Core Value Identity And Containers Fix

## Scope

This step repaired `CR001-B02-F001` and `CR001-B02-F002` in one related
foundation-value cluster. It changed one production header and one test source.
It did not alter string storage, container backing types, pointer ownership, or
Feature 003's process-local hash policy.

## Changes

- Added explicit `FName` copy and move operations.
- Move construction and move assignment now derive the destination hash from
  its resulting text and re-derive the moved-from source hash from the source's
  valid post-move text.
- Centralized hash-plus-text equality in `AreEqual`.
- Replaced `FromTextAndHashForTesting`, which returned invariant-breaking
  synthetic objects, with `CompareWithForcedCommonHashForTesting`, which
  exercises the collision path without allowing forged `FName` state to escape.
- Replaced the tautological moved-from `FString` assertion with a meaningful
  reassignment check.
- Added `FName` copy, collision, reflexive/symmetric/transitive equality,
  move-construction, and move-assignment tests for both destination identity and
  moved-from source text/hash consistency.

## Public API Migration

The test-only public helper changed:

```text
FromTextAndHashForTesting(text, hash) -> FName
CompareWithForcedCommonHashForTesting(left, right, common_hash) -> bool
```

Repository search found no production caller. The sole test caller was migrated
in the same commit. The replacement preserves collision verification while
preventing callers from constructing a name whose observable hash disagrees
with its text.

## Local Verification

- Fix commit: `0bfcdec`
- Strict Debug build with `-Werror`: PASS
- Focused C++20 invariant probe: `1 1 1`
- Core foundation suite: 60 passed, 0 failed
- Full deterministic suite with optional local deferred native execution
  isolated: PASS, exit code 0
- CR CLI tests: 19 passed
- `git diff --check`: PASS
- Removed factory and tautological assertion search: no matches

The ordinary local suite still reproduced accepted finding
`CR001-B08-F001`: deferred native submission and semantic probes failed under
MoltenVK while Core remained 60/0 and Triangle deterministic validation passed.
This pre-existing native result is retained in `gate-tests.json`; it does not
weaken or replace B02's deterministic evidence.

## Finding State

- `CR001-B02-F001`: Fixed; verification remains B02-S03.
- `CR001-B02-F002`: Fixed; verification remains B02-S03.

