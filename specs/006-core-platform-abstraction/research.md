# Research: Core Foundation - Platform Abstraction Layer

**Feature**: 006-core-platform-abstraction  
**Date**: 2026-06-24  
**Status**: Complete

## Research Tasks

This feature introduces the Core layer's platform abstraction layer: platform identity, platform information, high-resolution timing, local file operations, explicit-path dynamic module loading, and native window handle representation. The research phase resolves all planning choices needed before design and task generation.

---

## Decision 1: Platform Identity and Macros

**Decision**: Provide a single Core public platform macro header defining `SG_PLATFORM_WINDOWS`, `SG_PLATFORM_MAC`, and `SG_PLATFORM_LINUX` as mutually exclusive build-time constants.

**Rationale**: The constitution requires cross-platform behavior and isolated platform-specific code. A single macro header keeps platform detection consistent across Core and later phases. Mutually exclusive constants make it easy to assert that exactly one supported target is active.

**Alternatives Considered**:

- Ad hoc checks (`_WIN32`, `__APPLE__`, `__linux__`) throughout source files (rejected - duplicates platform decisions and leaks OS-specific naming into call sites).
- Runtime-only platform detection (rejected - conditional compilation is still needed for OS-specific dynamic loading and handle representation).

---

## Decision 2: Focused `FPlatform*` Types

**Decision**: Split responsibilities into focused public types: `FPlatformMisc`, `FPlatformTime`, `FPlatformFileSystem`, `FPlatformProcess`, and `FPlatformWindow`.

**Rationale**: The constitution prohibits god-classes. Separate types keep task boundaries, tests, and future replacement points small. This mirrors the roadmap deliverables and keeps each public contract easy to validate independently.

**Alternatives Considered**:

- One `FPlatform` facade with all methods (rejected - too broad and likely to grow into a god-class).
- Per-platform subclasses exposed publicly (rejected - exposes implementation shape and complicates call sites).

---

## Decision 3: High-Resolution Time Source

**Decision**: `FPlatformTime` exposes monotonic timestamps and duration conversion helpers. The implementation uses monotonic clock facilities on each platform and never uses wall-clock time for elapsed measurements.

**Rationale**: Spec SC-004 requires repeated time queries to never move backward. Wall-clock time can jump due to user or system time changes. Monotonic clocks are the correct basis for startup, frame, and test diagnostics.

**Alternatives Considered**:

- Wall-clock/system time (rejected - can move backward and is unsuitable for elapsed duration).
- CPU cycle counters as the primary contract (rejected - calibration and cross-platform consistency are unnecessary for this phase).

---

## Decision 4: Local File Operations

**Decision**: `FPlatformFileSystem` covers local file existence, byte-preserving read/write, and recursive directory creation. The implementation should use portable standard filesystem facilities when available and platform-specific fallback only if compiler support requires it.

**Rationale**: Requirements are intentionally small and local. Standard filesystem facilities are enough for existence checks, recursive directory creation, and simple local paths while keeping the public contract platform-neutral.

**Alternatives Considered**:

- Custom file abstraction with streaming, permissions, and watchers (rejected - outside current scope).
- Platform-only implementations from the start (rejected - adds duplication before it is necessary).

---

## Decision 5: Recursive Directory Creation

**Decision**: Directory creation creates all missing parent directories and reports whether the final directory is available.

**Rationale**: Clarification selected recursive behavior. This matches expected usage for test output, shader/cache directories, and future asset tooling paths. It avoids forcing every caller to split paths manually.

**Alternatives Considered**:

- Only create the final path segment if the parent exists (rejected in clarification - too brittle for expected engine use).
- Expose both recursive and non-recursive modes (rejected - unnecessary branch in the first Core PAL contract).

---

## Decision 6: Dynamic Module Loading Scope

**Decision**: `FPlatformProcess` loads dynamic modules only from explicit file paths. Bare module names and implicit platform search paths are out of scope.

**Rationale**: Clarification selected explicit-path loading. This improves determinism, avoids platform-specific search path behavior, and reduces accidental loading of an unintended module. It is also easier to test consistently.

**Alternatives Considered**:

- Allow platform search paths for bare module names (rejected - inconsistent and less safe).
- Restrict to engine-approved directories only (rejected - useful policy, but better deferred until plugin/package management exists).

---

## Decision 7: Native Window Handle Representation

**Decision**: `FPlatformWindow` represents native window handles as an opaque Core-owned value that can be empty, validity-checked, copied, and passed through public interfaces without including OS, windowing framework, or graphics API headers.

**Rationale**: The full window system belongs to Phase 015, but Vulkan swapchain and backend work need a stable representation for native handles. Opaque representation keeps Core decoupled and avoids exposing `HWND`, `NSWindow*`, X11/Wayland, GLFW, or graphics API types through Core headers.

**Alternatives Considered**:

- Include native OS handle headers directly in public Core headers (rejected - violates cross-platform isolation and causes include pollution).
- Defer window handle representation entirely (rejected - would block backend planning).

---

## Decision 8: Verification Strategy

**Decision**: Add Core platform verification to the existing `StonerTest` executable. Tests cover macro exclusivity, platform info sanity, monotonic timestamp samples, file roundtrip and recursive directory creation, dynamic module failure handling, and native window handle validity.

**Rationale**: This follows existing project practice from prior Core phases. No external test framework is required, and the full engine smoke test continues to be a single executable.

**Alternatives Considered**:

- Separate platform-specific test binaries (rejected - unnecessary fragmentation for current scope).
- Manual-only validation (rejected - requirements demand repeatable unit coverage).

---

## Open Questions

None. All planning decisions are resolved.
