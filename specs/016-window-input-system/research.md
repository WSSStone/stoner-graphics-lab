# Research: Window & Input System

## Decision: Public API is Application-level window/input/loop contracts with private driver strategies

**Rationale**: The feature belongs to the Application layer and must not expose platform libraries, native handles, RHI objects, swapchains, or renderer presentation details. A public `FWindow`/`FInputManager`/`FApplicationLoop` surface with private driver strategies keeps the API stable while allowing a deterministic headless driver and a real-window adapter.

**Alternatives considered**:

- Expose native platform handles in public contracts: rejected because it leaks platform details and would couple later rendering integration too early.
- Put window ownership in Renderer: rejected because window lifecycle and input are Application concerns.
- Use one combined application singleton: rejected because it would create a god-class and make input, window, and loop behavior harder to test independently.

## Decision: Deterministic headless validation is required

**Rationale**: The clarified spec requires deterministic validation when display access is unavailable. A headless driver can simulate lifecycle, resize, focus, close, keyboard, mouse, and pointer events without a display server, which makes CI and remote development reliable.

**Alternatives considered**:

- Require real windows for all tests: rejected because display access is not guaranteed in automated or remote environments.
- Defer real window behavior entirely: rejected because the roadmap phase must establish window creation and lifecycle, not only an input simulator.
- Let no-display runs fail hard: rejected because the spec requires controlled unavailable-runtime behavior.

## Decision: Use a GLFW-first real-window adapter when display access and dependency support are available

**Rationale**: The roadmap explicitly favors GLFW first to reach the triangle demo quickly. Keeping GLFW inside a private adapter allows the public Application contract to remain stable and cross-platform while real-window smoke tests run only when the dependency and display are available.

**Alternatives considered**:

- Native Win32/Cocoa/X11-Wayland implementations in v1: rejected because it expands scope and slows the first interactive milestone.
- Headless-only v1: rejected because it under-delivers the roadmap's window creation requirement.
- Public GLFW types: rejected because it would leak a third-party dependency through Application headers.

## Decision: Keyboard input represents physical key/button state only

**Rationale**: The user clarified that v1 input should track physical key and mouse-button state. This is the right foundation for games, camera controls, and deterministic tests; text input, IME composition, and keyboard-layout translation can be added later as separate behavior.

**Alternatives considered**:

- Include basic text character input: rejected because it adds layout and composition concerns outside v1 scope.
- Normalize by user-visible keyboard characters: rejected because it makes physical controls less stable across keyboard layouts.
- Store raw platform key codes only: rejected because Application callers need stable engine-level identifiers.

## Decision: Focus loss clears all held keys and mouse buttons immediately

**Rationale**: Clearing held state on focus loss prevents stuck inputs after Alt-Tab, window switching, or platform focus transitions. It also gives a simple, deterministic state transition for tests and runtime users.

**Alternatives considered**:

- Preserve held states until release events arrive: rejected because release events may never arrive after focus loss.
- Freeze the input snapshot and restore it on focus regain: rejected because it can reintroduce stale user intent.
- Clear only keyboard state: rejected because mouse buttons can become stuck for the same reason.

## Decision: Minimized or zero drawable size windows pause presentation, not event polling or updates

**Rationale**: A minimized or zero-size window should not force invalid presentation work, but the application still needs to process close, focus, input, and resize events. A `PresentationPaused` loop state creates a clean handoff for later renderer/swapchain integration.

**Alternatives considered**:

- Pause the entire application loop: rejected because the application could stop processing the restore or close event promptly.
- Treat zero size as fatal: rejected because minimization is a normal desktop window state.
- Keep presenting with zero size: rejected because later swapchain/presentation paths cannot reliably present without a drawable extent.

## Decision: Resize events coalesce to deterministic latest-size state per frame

**Rationale**: Desktop platforms can emit multiple resize events before the application polls. The Application layer should preserve event observability while ensuring the final per-frame window state is deterministic and reflects the latest client size.

**Alternatives considered**:

- Preserve every intermediate resize as final state: rejected because it causes stale final dimensions.
- Drop resize events completely and expose only current size: rejected because downstream systems need to know a resize occurred.
- Recreate presentation resources inside this feature: rejected because swapchain recreation belongs to a later rendering/demo integration phase.

## Decision: Diagnostics and debug dumps are first-class outputs

**Rationale**: The feature must be heavily testable without visual output. Structured diagnostics and deterministic debug dumps provide observable proof for invalid window configs, unavailable display, input transitions, focus loss, resize coalescing, loop exit, and presentation-paused behavior.

**Alternatives considered**:

- Visual inspection only: rejected because the headless path must be fully verifiable.
- Unstructured log text only: rejected because regression tests need stable categories and ordering.
- Rely only on boolean return values: rejected because callers need actionable failure information.
