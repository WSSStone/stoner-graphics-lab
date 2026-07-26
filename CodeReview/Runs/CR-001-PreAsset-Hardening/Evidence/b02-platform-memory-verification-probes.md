# B02-S21 Independent Probe Evidence

- Fix commit: `7a78cc6db8ed80c4b6d373cd932677850f58abbe`
- Verification HEAD: `21b2253fb350099340ac4cca40e8b94fed3ce94a`
- Host: macOS arm64
- Repository source changes from probes: none
- GitHub Actions used: none

## Parent Versus Current Platform Matrix

Current header:

```text
Platform identity matrix passed: Windows, macOS, Linux; Android, iOS, unknown rejected
exit=0
```

Parent header:

```text
ERROR: android did not fail with the unsupported-platform diagnostic
SG_PLATFORM_LINUX == 1
ERROR: ios did not fail with the unsupported-platform diagnostic
SG_PLATFORM_MAC == 1
pre_fix_matrix_exit=1 expected_nonzero=true
```

The parent source was read directly from `7a78cc6^`; no historical file was
rewritten.

## Parent Versus Current Mach Ownership

Parent implementation:

```text
queries=1024 refs_before=1 refs_after=1025 delta=1024 bytes=23146037248
pre_fix_mach_exit=4 expected=4
```

Current implementation:

```text
queries=1024 refs_before=1 refs_after=1 delta=0 bytes=23146037248
exit=0
```

Both binaries used the same probe source and public include directory. Only
the linked `FPlatformMisc.cpp` revision differed.

## Maintained Integration

The current strict Debug dependency tree includes:

```text
Build/Mac/Debug/Tests/PlatformIdentityMatrix.stamp
  +-Tests/verify_platform_identity.py
  +-Source/Core/Public/Core/SGPlatform.h
```

The full deterministic `StonerTest` run completed with exit `0`, including:

```text
Core platform tests passed=33 failed=0
Core platform ownership tests passed=3 failed=0
```
