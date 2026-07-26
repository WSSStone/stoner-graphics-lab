# B02-S21: Platform Selection And Memory Ownership Verification

## Verified Scope

This step independently reviews and exercises the repair in
`7a78cc6db8ed80c4b6d373cd932677850f58abbe` for:

- `CR001-B02-F016`: unsupported mobile targets selecting desktop identities;
- `CR001-B02-F017`: unbalanced macOS host send-right ownership.

The fix commit contains seven files: two production files, three test sources,
the test executable composition root, and its SCons integration. It does not
change window semantics, process RSS behavior, higher-layer APIs, or feature
specification history. `git show --check` passes.

## Platform Classification Verification

The committed compiler matrix passes against the repaired header:

```text
Platform identity matrix passed: Windows, macOS, Linux; Android, iOS, unknown rejected
```

The same script was then run against `SGPlatform.h` extracted from the fix
commit's parent. It exited `1` and identified both regressions:

```text
ERROR: android did not fail with the unsupported-platform diagnostic
SG_PLATFORM_LINUX expanded to 1

ERROR: ios did not fail with the unsupported-platform diagnostic
SG_PLATFORM_MAC expanded to 1
```

This proves the new check is sensitive to the original defect rather than
merely compiling a happy path.

SCons' target-specific dependency tree confirms:

```text
Build/Mac/Debug/Tests/StonerTest
  -> Build/Mac/Debug/Tests/PlatformIdentityMatrix.stamp
     -> Tests/verify_platform_identity.py
     -> Source/Core/Public/Core/SGPlatform.h
```

The matrix therefore reruns when either its implementation or the production
platform header changes. Both subprocess call sites use argument arrays and
`check=False`; neither uses a shell.

## Mach Ownership Verification

The focused 1,024-query probe was compiled separately against the parent and
current production implementations:

```text
parent:  refs_before=1 refs_after=1025 delta=1024 exit=4
current: refs_before=1 refs_after=1    delta=0    exit=0
```

The current full deterministic test executable also passes its three
maintained platform-ownership assertions and exits `0`.

## Existing Gate Recheck

- Strict Debug dependency/build check: passed.
- Full deterministic suite with the established optional-native skip: passed.
- B02-S20 strict Release gate: passed.
- B02-S20 strict ASan/UBSan build and deterministic suite: passed.
- Review state lint: passed.

No production or test source changed during this verification packet.

## Verification Boundary

The local evidence directly verifies the macOS ownership repair and the
compiler matrix's ability to reject the original Android/iOS behavior.
However, this step does not claim real MSVC or Linux-host execution. Per the
B02 protocol, `CR001-B02-F016` and `CR001-B02-F017` remain `Fixed` until the
batch-boundary push supplies successful Windows, macOS, and Linux hosted
evidence. No GitHub Actions run was requested here.
