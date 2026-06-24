# Feature Specification: Core Foundation - Platform Abstraction Layer

**Feature Branch**: `006-core-platform-abstraction`  
**Created**: 2026-06-24  
**Status**: Draft  
**Input**: User description: "Core platform abstraction layer: FPlatformProcess (dynamic library loading), FPlatformFileSystem (basic file I/O), FPlatformMisc (OS info queries), FPlatformWindow (native handle types), FPlatformTime (high-res timer). Platform detection macros (SG_PLATFORM_WINDOWS/MAC/LINUX). Conditional compilation. UE5 naming. Cross-platform Win/Mac/Linux. Unit tests."

## Clarifications

### Session 2026-06-24

- Q: Should dynamic module loading require explicit file paths or allow implicit platform search paths? -> A: Explicit file paths only.
- Q: Should directory creation create missing parent directories recursively or only create the final directory? -> A: Create missing parent directories recursively.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Engine Developer Uses Platform Capabilities Uniformly (Priority: P1)

An engine developer building Core, RHI, Backend, Renderer, or Application features needs a single engine-facing way to ask basic platform questions, measure elapsed time, and work with platform handles without scattering operating-system-specific decisions through higher layers.

**Why this priority**: Platform abstraction is on the critical path for the Vulkan backend and future windowing work. Without it, later phases would either duplicate platform checks or violate the constitution's cross-platform isolation rule.

**Independent Test**: Can be fully tested by querying platform identity, CPU count, memory availability, and a high-resolution timestamp from a Core-only test and verifying the results are present, sensible, and stable on the host platform.

**Acceptance Scenarios**:

1. **Given** a developer includes the Core public interface, **When** they request the current platform name, **Then** the system reports one supported platform identity without requiring the caller to know operating-system details.
2. **Given** a developer needs timing data, **When** they capture two timestamps around a short operation, **Then** the measured elapsed time is non-negative and precise enough for frame and startup diagnostics.
3. **Given** a developer needs platform environment information, **When** they query CPU count and available memory, **Then** the results are usable for diagnostics and fallback decisions.

---

### User Story 2 - Engine Developer Performs Basic File Operations Portably (Priority: P1)

An engine developer needs to read, write, check for, and create simple files and directories during tests, tooling, and early engine startup. They use the platform abstraction so the same engine-level operation works across Windows, macOS, and Linux.

**Why this priority**: File access is foundational for loading configuration, shader assets, test fixtures, and future content. Portable file behavior prevents platform-specific assumptions from leaking into later systems.

**Independent Test**: Can be fully tested by creating a temporary directory, writing a small text or binary payload, checking that it exists, reading it back, and verifying the exact content matches.

**Acceptance Scenarios**:

1. **Given** a writable temporary location, **When** a developer creates a directory and writes a small payload into a file, **Then** the file exists and can be read back without content changes.
2. **Given** a missing file path, **When** a developer asks whether the file exists or attempts to read it, **Then** the system reports failure clearly without crashing.
3. **Given** a nested directory request with missing parent directories, **When** the developer asks the system to create it, **Then** every missing parent directory is created and the final directory is available for subsequent file operations.

---

### User Story 3 - Engine Developer Loads Optional Runtime Modules (Priority: P2)

An engine developer working on backend or tooling experiments needs to load an optional runtime module, look up a named entry point, and release the module when finished. This allows later phases to integrate platform-specific or optional functionality behind a common engine-facing contract.

**Why this priority**: Dynamic loading is important for backend extensibility and diagnostics, but the engine can still build early milestones without it if basic platform and file services are available first.

**Independent Test**: Can be fully tested by attempting to load a known existing runtime module on the host platform, resolving a known symbol when available, and verifying that missing modules and missing symbols are reported as failures rather than crashes.

**Acceptance Scenarios**:

1. **Given** a valid runtime module path for the current platform, **When** a developer loads it, **Then** the system returns a valid module handle that can be released.
2. **Given** a loaded module and an existing exported entry point, **When** a developer requests the entry point by name, **Then** the system returns a callable address or equivalent handle.
3. **Given** a missing module or missing entry point, **When** a developer requests it, **Then** the system returns a clear failure result and leaves the engine in a valid state.

---

### User Story 4 - Engine Developer Passes Native Window Handles Safely (Priority: P2)

An engine developer preparing for backend and application integration needs to represent native window handles in a Core-owned type so higher layers can pass platform window references without depending directly on one operating system's handle type.

**Why this priority**: The full windowing system is deferred to a later phase, but backend work needs a stable representation for native handles before swapchain and surface integration begins.

**Independent Test**: Can be fully tested by constructing empty and platform-specific native handle values in Core-level tests and verifying they can be copied, compared for validity, and passed through public interfaces without including higher-layer or graphics API headers.

**Acceptance Scenarios**:

1. **Given** no native window has been created, **When** a developer constructs an empty window handle, **Then** it is reported as invalid but safe to store and pass around.
2. **Given** a native handle value supplied by a future windowing layer, **When** a developer wraps it in the platform abstraction, **Then** the value remains retrievable for platform-specific backend use.
3. **Given** Core platform headers are included in isolation, **When** the project is built, **Then** no RHI, Backend, Renderer, Application, or graphics API dependency is required.

---

### Edge Cases

- What happens when a platform cannot provide an optional capability such as available memory? The system reports an explicit unavailable or zero-equivalent result while keeping the caller safe.
- What happens when file paths contain spaces or non-ASCII characters? The system must preserve the path value and either complete the operation or return a clear failure without corrupting the path.
- What happens when a file read is attempted on a directory or inaccessible path? The operation fails without partial success being reported.
- What happens when a dynamic module is loaded more than once or released after a failed load? Valid handles remain manageable, and invalid handles are safe no-ops.
- What happens when the platform clock is queried repeatedly in rapid succession? Returned time values never move backward within a single process run.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: This feature is in the Core layer and MUST NOT depend on RHI, Backend, Renderer, Application, or any graphics API.
- **Design Patterns**: Platform-specific behavior MUST be isolated behind focused platform capability boundaries. The feature MUST avoid a single catch-all god-class that owns unrelated responsibilities.
- **Advanced Graphics**: The feature MUST provide stable timing and handle abstractions suitable for later rendering, backend, and frame diagnostics work without coupling Core to those systems.
- **Naming Conventions**: Public names MUST follow UE5-style naming conventions, including `F` prefixes for value/service types and `SG_` prefixes for platform detection macros.
- **Cross-Platform Compatibility**: The feature MUST support Windows, macOS, and Linux. Platform-specific behavior MUST be isolated behind platform guards or equivalent abstraction boundaries so call sites remain portable.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST expose exactly one supported platform identity for the active build among Windows, macOS, and Linux.
- **FR-002**: System MUST provide platform detection macros or equivalent public constants for Windows, macOS, and Linux that allow platform-conditional code to be expressed consistently.
- **FR-003**: System MUST provide a platform information capability that reports the operating system name, CPU core count, and available memory when the host platform can provide it.
- **FR-004**: System MUST provide a high-resolution time capability that returns monotonic timestamps suitable for measuring elapsed time during engine startup, tests, and frame diagnostics.
- **FR-005**: System MUST provide elapsed-time conversion helpers so developers can compare durations in seconds, milliseconds, and microseconds without platform-specific call sites.
- **FR-006**: System MUST provide basic file existence checks for files and directories using engine-facing path values.
- **FR-007**: System MUST provide basic file read and write operations that preserve byte-for-byte content for small text and binary payloads.
- **FR-008**: System MUST provide recursive directory creation for a requested path, including missing parent directories, and report whether the final directory is available after the operation.
- **FR-009**: System MUST report file operation failures clearly without crashing, leaking resources, or leaving partially successful results marked as complete.
- **FR-010**: System MUST provide a dynamic module loading capability that can load a runtime module by explicit file path, resolve an entry point by name, and release a valid loaded module. Bare module names and implicit platform search paths are out of scope.
- **FR-011**: System MUST treat missing modules, missing entry points, and invalid module handles as recoverable failures.
- **FR-012**: System MUST provide a native window handle abstraction that can represent an empty handle and supported platform handle values without exposing higher-layer dependencies.
- **FR-013**: System MUST ensure Core platform public headers compile in isolation and do not include RHI, Backend, Renderer, Application, windowing framework, or graphics API headers.
- **FR-014**: System MUST provide unit-test coverage for platform identity, platform information, high-resolution timing, file operations, dynamic module failure handling, and native window handle validity.
- **FR-015**: System MUST preserve existing Core foundation, math, logging, and assertion behavior while adding the platform abstraction.

### Key Entities

- **Platform Identity**: The active supported operating-system family used for diagnostics and platform-conditional behavior.
- **Platform Information**: A snapshot of host details such as operating system name, CPU core count, and memory availability.
- **Platform Timestamp**: A monotonic time value used to measure elapsed durations.
- **File Operation Result**: The success or failure state and payload for basic file existence, read, write, and directory operations.
- **Dynamic Module Handle**: A managed representation of a loaded optional runtime module or an invalid load result.
- **Native Window Handle**: A Core-owned representation of an operating-system window reference for future backend and application integration.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can query platform identity, CPU core count, memory availability, and monotonic time from a Core-only test in under 5 minutes without adding platform-specific code at the call site.
- **SC-002**: File write-read verification preserves 100% of bytes for representative text and binary payloads up to 1 MB on each supported platform.
- **SC-003**: Missing file, inaccessible path, missing module, and missing entry point cases fail deterministically 100% of the time without crashing.
- **SC-004**: Repeated time queries over at least 1,000 samples never move backward within a single process run.
- **SC-005**: Core platform public headers compile without importing RHI, Backend, Renderer, Application, windowing framework, or graphics API dependencies.
- **SC-006**: The feature passes the existing full engine test executable together with new platform abstraction tests on Windows, macOS, and Linux.
- **SC-007**: All platform needs identified in the Phase 008 Vulkan device/swapchain and Phase 015 application window/input roadmap prompts are represented by documented Core platform contracts without requiring higher-layer public dependencies.

## Assumptions

- The Core foundation, math, logging, and assertion phases are complete and available as prerequisites.
- The full window creation and input system remains out of scope for this phase; only native handle representation is included.
- Threading primitives, networking, environment variable management, process spawning, and command-line parsing are out of scope for this phase.
- File operations are limited to basic local file and directory access needed by tests, startup, and early tooling.
- Dynamic loading support targets optional runtime modules and backend/tooling extensibility; plugin lifecycle management is out of scope.
- Platform-specific implementation choices will be resolved during planning while keeping the public behavior described here stable.
