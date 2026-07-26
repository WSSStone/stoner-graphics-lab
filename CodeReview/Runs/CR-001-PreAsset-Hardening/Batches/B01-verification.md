# B01-S03: Build, CI, and Architecture Verification

## Verified Head

- Commit: `8a3283872b3e361ec5098f438e67467e26205851`
- Main CI run: `30186757383`
- Code Review Tools run: `30186757380`
- Trigger: `pull_request`

The verified head produced exactly two workflow runs: one `CI` run and one
`Code Review Tools` run. There was no branch `push` run.

## Final Matrix

All eight checks passed:

1. Windows headless strict Debug and deterministic validation
2. macOS headless strict Debug and deterministic validation
3. Linux headless strict Debug, deterministic validation, and required
   Lavapipe native validation
4. Windows strict Release
5. macOS strict Release
6. Linux strict Release
7. Linux ASan plus UBSan build and tests
8. CR CLI tests

## Remote-Discovered Corrections

The first strict matrix exposed a fallback compile error because report
dimensions had been moved into a native-only preprocessor scope. The constants
now remain visible to both native and fallback report generation.

To prevent recurrence, SCons now supports `graphics=disabled`, and `crctl gate
fallback-strict` performs a strict dependency-free build and executes its test
binary even on SDK-equipped machines.

The next matrix exposed Linux runtime/development loader confusion:
`libvulkan.so.1` was accepted although the build links with `-lvulkan`.
Detection now requires the unversioned development link. Required Linux native
CI installs `libvulkan-dev` and remained green.

The final Release-only diagnostic was an unchecked `fread` result in logging
test capture. The capture now validates seek/size operations, handles short
reads, and returns only initialized bytes.

## Local Evidence

- Strict native Debug: PASS
- Strict native Release: PASS
- Explicit fallback strict build and tests: PASS
- Release tests with optional MoltenVK execution isolated: PASS
- CR tool tests: 16 PASS
- Invalid graphics mode: rejected explicitly

The ordinary macOS native suite reproduced existing finding
`CR001-B08-F001`: one deferred report failed, and the immediate rerun passed;
both completed native submission and released all live objects. The two
reports are retained for B08 and do not weaken the required Linux Lavapipe
native gate, which passed remotely.

## Finding Decisions

- `CR001-B01-F001`: Verified
- `CR001-B01-F002`: Verified
- `CR001-B01-F003`: Verified
- `CR001-B09-F001`: remains Accepted for B09
- `CR001-B08-F001`: remains Accepted for B08
