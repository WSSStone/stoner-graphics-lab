# B02-S19 Platform And Memory Probe Evidence

- Probe sources: `/tmp/cr001_b02_s19_*.cpp`
- Host: macOS arm64
- Repository production changes from probes: none
- GitHub Actions used: none

## Platform Identity Matrix

The probe maps Stoner identities to exit codes:

```text
Windows=100
Mac=10
Linux=1
```

Observed compiler simulations against the production header:

| Simulated target | Relevant macros | Compile | Exit | Result |
|---|---|---:|---:|---|
| Native macOS | compiler defaults | pass | `10` | correct macOS |
| Android | `__ANDROID__`, `__linux__` | pass | `1` | incorrectly Linux |
| iOS | `__APPLE__`, `__MACH__`, `TARGET_OS_IPHONE=1` | pass | `10` | incorrectly macOS |
| Generic unknown | supported OS macros undefined | fail | n/a | intended `#error` |

The generic failure included:

```text
Unsupported platform for Stoner Graphics Lab
```

## Mach Host Right Ownership

The probe held one host send right, recorded its user-reference count, called
the production `FPlatformMisc::GetAvailableMemoryBytes()` 1,024 times, and
recorded the count again:

```text
queries=1024 refs_before=1 refs_after=1025 delta=1024 bytes=23405150208
exit=4
```

The exact one-reference-per-query growth demonstrates unbalanced ownership of
the send right returned by `mach_host_self()`.

## Concurrent Query Stress

Eight threads each performed 2,048 process RSS and available-memory queries
against the production implementations under ThreadSanitizer:

```text
expected=16384 process=16384 available=16384
exit=0
```

ThreadSanitizer emitted no race report. This confirms current concurrent query
behavior on macOS while leaving the separate Mach right leak reproducible.

## Window Handle Trace

The maintained real-window path is:

```text
FGlfwWindowDriver::GetPlatformWindow
  -> FWindow::PlatformWindow
  -> FVulkanNativeContext::Initialize
  -> glfwCreateWindowSurface
```

`FWindow::Destroy()` destroys and resets the driver before clearing the
non-owning platform handle. The inspected path therefore did not produce an
additional finding.
