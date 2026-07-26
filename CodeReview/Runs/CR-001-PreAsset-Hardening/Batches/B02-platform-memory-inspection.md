# B02-S19: Platform Selection, Window, Misc, And Memory Inspection

## Inspection Budget

The inspection covered one Core platform responsibility domain and seven
production files, totaling 277 production lines:

1. `Source/Core/Public/Core/SGPlatform.h`
2. `Source/Core/Public/Core/FPlatformTypes.h`
3. `Source/Core/Public/Core/FPlatformWindow.h`
4. `Source/Core/Public/Core/FPlatformMisc.h`
5. `Source/Core/Private/FPlatformMisc.cpp`
6. `Source/Core/Public/Core/FPlatformMemory.h`
7. `Source/Core/Private/FPlatformMemory.cpp`

Supporting evidence included Feature 006 and Feature 018 requirements,
contracts, data models, maintained Core platform tests, current GLFW and Vulkan
call sites, repository history, compiler-macro target simulations, a direct
Mach port ownership probe, and a ThreadSanitizer stress probe. No production
implementation was changed.

## Requirement Mapping

- `006-FR-001` and `006-FR-002` require exactly one supported
  Windows/macOS/Linux identity and a clear compile-time failure for unsupported
  targets.
- `006-FR-003` requires OS, processor-count, and available-memory queries. A
  query may return zero when the operating system cannot provide a value, but
  it must not leak operating-system resources.
- `006-FR-012` through `006-FR-014` require an opaque, copyable, Core-only
  platform-window value and direct platform-contract coverage.
- Feature 018 uses `FPlatformMemory::QueryProcessMemory` for maintained
  endurance RSS sampling.

## Findings

### CR001-B02-F016 - Accepted S2

`SGPlatform.h` accepts `__linux__` without excluding Android and accepts the
generic Apple/Mach macro pair without checking the Apple target family.
Compiler-macro simulations therefore classify Android as Linux and iOS as
macOS instead of rejecting unsupported targets. A truly unknown target does
reach the intended compile-time error.

### CR001-B02-F017 - Accepted S2

The macOS branch of `FPlatformMisc::GetAvailableMemoryBytes` obtains a send
right with `mach_host_self()` but never balances it with
`mach_port_deallocate`. A probe around the production function observed a
one-for-one increase in the host port's send-right user-reference count:
1,024 calls increased the count by 1,024.

## Confirmed Strengths

- A truly unknown platform fails compilation rather than selecting a fallback.
- The native macOS build defines exactly one Stoner platform identity.
- Fixed-width scalar aliases and pointer-width integer aliases use standard
  types with the expected widths.
- `FPlatformWindow` is a small non-owning value wrapper. The current
  Application path stores a live `GLFWwindow*`, clears it after driver
  destruction, and passes it through the Core boundary only while valid.
- The Vulkan bridge consumes that GLFW pointer behind the Backend private
  implementation boundary; no direct GLFW type leaks into Core public headers.
- `FPlatformMemory::QueryProcessMemory` uses the supported process RSS API on
  each desktop platform and reports an explicit availability flag.
- Eight concurrent threads completed 16,384 process-memory and 16,384
  available-memory queries under ThreadSanitizer with no race report.
- `mach_task_self()` in the process-memory query is a task self special port
  and does not have the ownership defect reproduced for `mach_host_self()`.

## Coverage Gaps

- `TestPlatformIdentity` validates only the compiler's current host macro set;
  it cannot catch Android or non-macOS Apple target classification.
- `TestPlatformMisc` checks only that available memory is zero or plausible;
  it does not repeat the macOS query while accounting for Mach rights.
- `TestPlatformMemory` checks a single process RSS sample rather than repeated
  sampling, although the independent concurrency probe found no race.
- The Core window unit test uses a synthetic pointer. Application and native
  integration tests provide the maintained real GLFW bridge coverage.

## B02-S20 Fix Packet

The next packet may repair only the two accepted findings:

1. Reject Android before Linux and accept only macOS within the Apple target
   family, with a maintained compile-only platform identity matrix.
2. Balance the macOS host send right on every return path and add repeated
   ownership regression coverage without changing Windows or Linux behavior.
