# B02-S03: Core Value Identity And Containers Verification

## Verified Heads

- Fix commit: `0bfcdec76013c302616cfc0c7cfdb4af5a6fd5b2`
- Remote evidence head: `2a97649d67d4197851b7e41b86e0e707dc4a2326`
- CI run: `30188865674`
- Code Review Tools run: `30188865675`
- Trigger: `pull_request`

The remote evidence head contains the fix commit plus only its CR report/state
commit.

## Remote Matrix

All eight required checks passed:

1. Windows strict Debug build and deterministic validation
2. macOS strict Debug build and deterministic validation
3. Linux strict Debug, deterministic validation, and required Lavapipe native
   validation
4. Windows strict Release build
5. macOS strict Release build
6. Linux strict Release build
7. Linux ASan plus UBSan build and test suite
8. CR tool tests

The PR check summary reports every check in the `pass` bucket. No failed step
appears in the structured run metadata.

## Local Re-Check

- Strict Debug build with warnings as errors: PASS
- Core foundation suite: 60 passed, 0 failed
- Full deterministic test suite with the already accepted optional local
  deferred-native path isolated: PASS
- Focused move/collision probe: `1 1 1`
- CR tool tests: 19 passed
- Patch whitespace validation: PASS
- Production/test patch scope: only `FName.h` and
  `CoreFoundationTests.cpp`
- `FromTextAndHashForTesting`: absent
- Tautological moved-from `IsEmpty() || !IsEmpty()` assertion: absent

The unisolated local MoltenVK run still maps exclusively to accepted
`CR001-B08-F001`; the remote Linux native gate passed and no B02 Core test
failed.

## API And Invariant Review

- Every normally constructed, copied, moved, or move-assigned `FName` derives
  its hash from its current text.
- The moved-from source remains a valid name whose equality agrees with a name
  reconstructed from its current text.
- Hash mismatch remains a fast inequality path for valid names.
- Equal forced hashes still require text equality.
- Collision testing no longer returns an invariant-breaking `FName`.
- Repository search found no unmigrated caller of the removed test-only
  factory.

## Finding Decisions

- `CR001-B02-F001`: Verified.
- `CR001-B02-F002`: Verified.

