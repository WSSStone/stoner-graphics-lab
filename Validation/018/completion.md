# Feature 018 Completion Record

Status: In progress

## Verified

- Local macOS debug build succeeds with SCons 4.10.1 through the `godot` conda environment.
- Full `StonerTest` regression suite passes locally.
- Deterministic 4,096-frame validation passes with 28 memory samples and zero final live objects.
- Checked-in vertex and fragment SPIR-V payloads pass `spirv-val`.
- Native Vulkan offscreen execution completes 4,096 frames through MoltenVK/Metal on an Apple M4 Pro outside the automation sandbox, with 28 memory samples and zero final live objects.
- The same native probe returns controlled runtime-unavailable inside the Metal-inaccessible sandbox and never silently falls back.
- Formal macOS run `macos-018-75f1e38-20260722T232702` completed naturally on an Apple M4 Pro with 10,000 of 10,000 frames, first presentation at 335.915 milliseconds, 22 recoveries with a 2.649-millisecond maximum, 75 RSS samples with 99,516,416-byte and 122,945,536-byte medians, and zero final live objects. Its same-run screenshot visibly shows one non-degenerate RGB-interpolated triangle without unrelated desktop content.
- Windows debug build succeeds with Visual Studio Community 2026 (MSVC 19.51.36248), Vulkan SDK 1.4.350.0, and the persistent GLFW 3.4 package at `D:\Programs\glfw-3.4`; the full `StonerTest.exe` regression suite passes.
- Windows deterministic validation completes 4,096 of 4,096 frames with 28 memory samples, a 9,293,824-byte baseline median, a 9,297,920-byte final median, and zero final live objects.
- Historical Windows run `windows-018-682e236-20260721T112047` completed the formal profile and its screenshot shows one non-degenerate RGB-interpolated triangle. Because the visible frame path was subsequently routed through Renderer/RHI bindings, this pair is retained as development evidence but must be regenerated from the current implementation before final acceptance.
- The current implementation prepares a real forward frame in Renderer, records it through `FForwardFrameExecutor` into native RHI bindings, and submits/presents it through two rotating frame slots with image-indexed render-finished synchronization.
- Private-driver, demo orchestration, SPIR-V stage/entry-point, swapchain generation/stale-image, logical/framebuffer-size, minimize/restore, and close/Escape regression tests pass locally.
- Three-platform deterministic and Linux Lavapipe validation jobs are configured in `.github/workflows/ci.yml`.

## Requirement Reconciliation

| Requirements | Status | Evidence or remaining gate |
|---|---|---|
| FR-001, FR-004 through FR-006 | Complete | Standalone target, checked-in validated shaders, vertex upload, and real offscreen Vulkan draw are implemented and tested. |
| FR-002 | Complete on current macOS; Windows pending | GLFW creates and owns the primary visible window; current-path macOS evidence passes and Windows must be refreshed. |
| FR-003, FR-007, FR-008 | Complete on current macOS; Windows pending | The demo builds a Renderer forward plan, consumes backend-neutral native RHI bindings through `FForwardFrameExecutor`, then submits/presents in Vulkan without leaking native types upward. |
| FR-009, FR-010 | Complete on current macOS; Windows pending | The visible loop polls events, acquires, records through Renderer/RHI, rotates two frame slots, submits with image-indexed synchronization, and presents through the real swapchain. |
| FR-011 through FR-013 | Complete on current macOS; Windows pending | Generation-aware recreation, zero-extent pause, and 22 real macOS recoveries pass with a 2.649-millisecond maximum. |
| FR-014 through FR-017 | Complete for implemented modes | Strict CLI, bounded validation, reverse shutdown, stable diagnostics, failure injection, deterministic coverage, and interactive visible execution pass. |
| FR-018 | Partial | Three-platform workflow and Linux Lavapipe job are configured; green CI identity plus fresh Windows/macOS evidence are pending after the visible-path integration. |
| FR-019 through FR-021 | Complete | Memory/live-object gates, full local regressions, and the hardcoded-triangle scope boundary are covered. |
| SC-001 | Complete on current macOS; Windows pending | macOS first presentation passes at 335.915 milliseconds with matching screenshot/log evidence. |
| SC-002, SC-004 through SC-006, SC-008, SC-009 | Complete for local applicable paths | Deterministic endurance/failure tests pass; native macOS offscreen endurance also passes. |
| SC-003 | Complete on current macOS; Windows pending | Deterministic timing boundaries pass and all 22 macOS recoveries are below 2,000 milliseconds. |
| SC-007 | Pending | Requires a green identified CI matrix, Linux Lavapipe artifact, and both visible platform evidence pairs. |

## External Completion Gates

- GitHub Actions run/commit identity and Linux Lavapipe artifact have not yet been inspected.
- macOS run `macos-018-75f1e38-20260722T232702` has a matching screenshot and normalized passing log; both were manually inspected.
- Windows run `windows-018-682e236-20260721T112047` has a matching local screenshot and normalized log, but predates the Renderer/RHI visible-path integration and must be regenerated.
- The real Windows 10,000-frame/20-cycle validation must be repeated on the current implementation.
