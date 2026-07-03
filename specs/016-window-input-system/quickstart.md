# Quickstart: Window & Input System

## Goal

Validate that the Application layer can create and manage one primary window, derive deterministic physical keyboard/mouse frame state, process resize/close/focus events, pause presentation for minimized or zero drawable size windows, and run a minimal loop skeleton without requiring renderer presentation or real GPU work.

## Expected Development Flow

1. Add Application public contracts under `Source/Application/Public/Application/`.
2. Add private implementation files under `Source/Application/Private/`.
3. Export window/input/loop headers through `ApplicationMinimal.h` or direct public includes.
4. Add deterministic headless driver behavior for automated tests.
5. Add optional GLFW-backed real-window adapter behind private implementation boundaries when the dependency and display access are available.
6. Update `Source/Application/SConscript` only as needed for optional real-window dependency detection and platform link flags.
7. Add focused tests in `Tests/ApplicationWindowInputTests.cpp` and wire them into `Tests/Main.cpp`.
8. Keep public Application headers free of native window, GLFW, graphics API, swapchain, and renderer presentation types.

## Representative Headless Scenario

Create a deterministic window/input fixture with:

- One valid window description with title, positive client size, windowed mode, and visible state.
- One primary headless window driver.
- A creation event, several resize events before one frame, a minimized or zero drawable event, a restore event, focus lost/gained events, and a close-request event.
- Keyboard events for key down, repeated held frames, key up, and focus-loss clearing.
- Mouse button events for down, held, up, and focus-loss clearing.
- Pointer movement events with current position and per-frame delta.
- Unknown key/button events that must not corrupt known input state.
- A minimal loop that runs until close request or for a configured frame count.

Expected results:

- Valid window creation succeeds and exposes Active lifecycle state.
- Invalid window descriptions are rejected with diagnostics and no partially active window.
- Resize notifications are observable no later than the next poll; final per-frame size is the latest resize.
- Minimized or zero drawable size sets presentation-paused while polling and update processing continue.
- Focus loss immediately clears all held keyboard and mouse-button state.
- Pressed/released transient states reset at frame boundaries while held state persists until release or focus loss.
- Unknown input is reported without corrupting known input state.
- Close request is observable and allows clean loop exit.
- Debug dumps are byte-identical across 20 repeated runs of unchanged scenarios.

## Optional Real-Window Smoke Scenario

When display access and the real-window dependency are available:

- Create one primary visible window.
- Poll at least one frame.
- Observe active lifecycle, current client size, and focus/visibility state.
- Request close.
- Destroy the window cleanly.

Expected results:

- Smoke validation passes on display-backed environments.
- No-display or dependency-unavailable environments report a controlled unavailable/skipped result, not a crash.

## Negative Scenarios

Cover at minimum:

- Zero, negative, 16384x16384 maximum-boundary, and above-maximum window dimensions.
- Polling events before window creation.
- Polling events after window destruction.
- Destroy called more than once.
- Close requested while input events are pending.
- Several resize events before one frame poll.
- Minimized or zero drawable size.
- Fullscreen-style mode request failure that preserves previous window state.
- Focus loss while keys or mouse buttons are held.
- Pointer movement before an initial pointer position is known.
- Unknown key or mouse-button identifiers.
- Display unavailable for real-window creation.

## Verification Commands

```bash
conda run -n godot scons
Build/Mac/Debug/Tests/StonerTest
```

Latest local verification on 2026-07-03:

- `conda run -n godot scons`: PASS on macOS debug build.
- `Build/Mac/Debug/Tests/StonerTest`: PASS, including 25 Application window/input assertions plus existing Core/RHI/Renderer/Vulkan regression coverage.
- Public Application boundary grep: PASS with no public GLFW, native handle, graphics API, swapchain, or renderer presentation leakage.
- Real-window smoke path: dependency/display-backed execution is unavailable in this environment and reports controlled unavailable-runtime diagnostics; deterministic headless tests are the required default path.

On GitHub Actions or equivalent CI, run the same headless build/test validation on a Windows, macOS, and Linux matrix. The workflow should:

- Trigger on pull requests and pushes to the default integration branch.
- Use `ubuntu-latest`, `macos-latest`, and `windows-latest` runners.
- Build with SCons on every runner.
- Run the platform-specific `StonerTest` executable path produced by the build.
- Keep real-window smoke tests opt-in or unavailable-safe so no-display CI workers still pass deterministic headless validation.
- If any platform cannot be automated temporarily, record the gap, fallback manual verification command, and follow-up task before considering the feature complete.

The repository workflow at `.github/workflows/ci.yml` defines `ubuntu-latest`, `macos-latest`, and `windows-latest` jobs that install SCons, build the project, and run the platform-specific `StonerTest` executable.

## Optional Smoke Configuration

Real-window smoke tests should be opt-in or automatically skipped when the execution environment lacks display access. A later implementation may use a clearly named environment flag or test filter, but the required behavior is:

- Headless tests always run.
- Real-window smoke tests run only when display access and the real-window dependency are available.
- Unavailable display/dependency produces a deterministic skipped or unavailable diagnostic.

## Boundary Check

Public Application headers should not expose backend, graphics API, swapchain, renderer presentation, or third-party window types:

```bash
rg -n "\bGLFW\b|\bVulkan\b|\bMetal\b|\bDX12\b|\bDirectX\b|\bOpenGL\b|\bGLES\b|\bWebGL\b|\bVk[A-Z]\w*|\bID3D\w*|\bMTL\w*|\bSwapchain\b|\bHWND\b|\bNSWindow\b|\bX11\b|\bWayland\b" Source/Application/Public/Application
```

Private implementation files may mention a real-window adapter dependency only behind build detection or conditional compilation, and tests must still pass through deterministic headless validation when that dependency is unavailable.
