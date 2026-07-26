# B01-S02: Build, CI, and Architecture Fix

## Scope

This step repaired the three accepted B01 findings. It did not change the
test/private-header boundary assigned to B09 or attempt to solve the
intermittent MoltenVK deferred validation assigned to B08.

## Changes

- Added a zero-initializing Vulkan structure factory and migrated project-owned
  native Vulkan create-info initialization to it. This removes Clang/GCC
  missing-field warnings without suppressing the diagnostic.
- Removed one Windows-only unreachable branch and replaced an always-true
  unsigned memory assertion with a meaningful availability/plausibility check.
- Added validated `strict` and `sanitizers` SCons controls. Strict mode maps to
  `-Werror` or `/WX`; the sanitizer profile maps to ASan plus UBSan on Clang
  and GCC Debug builds.
- Added allow-listed strict Debug, strict Release, and sanitizer profiles to
  `crctl gate`, with unit coverage for platform-specific sanitizer behavior.
- Expanded hosted CI to strict Debug on Windows/macOS/Linux, strict Release on
  all three platforms, and Linux ASan/UBSan.
- Restricted branch `push` CI to `master`; pull requests remain validated.
  Pure `CodeReview/Runs/**` state updates are ignored, while CR tool and engine
  changes still trigger hosted gates.
- Added an explicit sanitizer-only escape hatch for optional deferred driver
  execution. Required native validation cannot use it, so the Linux Lavapipe
  native gate remains mandatory and no semantic oracle replaces native
  evidence.
- Updated the historical build contract and quickstart with the new public
  SCons arguments.

## Local Verification

- `strict-debug`: PASS with `-Werror`
- `strict-release`: PASS with `-Werror`
- `sanitizers`: PASS with ASan plus UBSan
- Ordinary macOS native test suite: PASS without the optional-native skip
- Deferred readback: PASS on Apple M4 Pro, 24 probes, zero final live objects
- CR CLI tests: 13 PASS
- CR state lint: PASS
- Invalid `strict` and Release-plus-sanitizer combinations: rejected
- GitHub Actions YAML parse: PASS
- CodeGraph: 309/309 project C/C++ files, 100% coverage

## Remaining Verification

B01-S03 must validate the pushed head on Windows, macOS, and Linux, including
the Linux sanitizer job and the non-duplicated workflow trigger. Findings
CR001-B01-F001 through F003 remain unverified until that remote evidence is
recorded.
