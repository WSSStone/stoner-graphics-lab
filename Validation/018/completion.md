# Feature 018 Completion Record

Status: In progress

## Verified

- Local macOS debug build succeeds with SCons 4.10.1 through the `godot` conda environment.
- Full `StonerTest` regression suite passes locally.
- Deterministic 4,096-frame validation passes with 28 memory samples and zero final live objects.
- Checked-in vertex and fragment SPIR-V payloads pass `spirv-val`.
- Native Vulkan offscreen execution completes 4,096 frames through MoltenVK/Metal on an Apple M4 Pro outside the automation sandbox, with 28 memory samples and zero final live objects.
- The same native probe returns controlled runtime-unavailable inside the Metal-inaccessible sandbox and never silently falls back.
- Real GLFW-backed Vulkan surface/swapchain presentation visibly renders one non-degenerate RGB-interpolated triangle on macOS. The inspected smoke screenshot is not retained as formal evidence because it contains unrelated desktop content and was not paired with the required 10,000-frame/20-recovery run.
- Windows debug build succeeds with Visual Studio Community 2026 (MSVC 19.51.36248), Vulkan SDK 1.4.350.0, and the persistent GLFW 3.4 package at `D:\Programs\glfw-3.4`; the full `StonerTest.exe` regression suite passes.
- Windows deterministic validation completes 4,096 of 4,096 frames with 28 memory samples, a 9,293,824-byte baseline median, a 9,297,920-byte final median, and zero final live objects.
- Windows run `windows-018-682e236-20260721T112047` completes the formal 10,000-frame visible validation on an NVIDIA GeForce RTX 3080 (driver 581.32): first presentation is 1,162.517 milliseconds, all 20 minimize/restore recoveries complete (maximum 58.457 milliseconds), RSS medians are 142,389,248 and 189,550,592 bytes, and final live objects are zero. The same-run local screenshot was manually inspected and shows one non-degenerate RGB-interpolated triangle.
- Three-platform deterministic and Linux Lavapipe validation jobs are configured in `.github/workflows/ci.yml`.

## Requirement Reconciliation

| Requirements | Status | Evidence or remaining gate |
|---|---|---|
| FR-001, FR-004 through FR-006 | Complete | Standalone target, checked-in validated shaders, vertex upload, and real offscreen Vulkan draw are implemented and tested. |
| FR-002 | Complete on local Windows and macOS | GLFW creates and owns the primary visible window; visible RGB pixels were inspected on both platforms. |
| FR-003, FR-007, FR-008 | Partial | Renderer/RHI forward execution contracts and tests exist, and the native context presents real pixels, but the visible frame still records its draw directly in the backend rather than consuming `FForwardFrameExecutor` RHI bindings. |
| FR-009, FR-010 | Complete on local Windows and macOS | The visible loop polls events, acquires, records, submits with semaphore/fence coordination, and presents through the real swapchain. |
| FR-011 through FR-013 | Partial | Real generation-aware recreation and zero-extent pause code is implemented; deterministic boundaries and the Windows 20-cycle validation pass, but the formal macOS 20-cycle run is pending. |
| FR-014 through FR-017 | Complete for implemented modes | Strict CLI, bounded validation, reverse shutdown, stable diagnostics, failure injection, deterministic coverage, and interactive visible execution pass. |
| FR-018 | Partial | Three-platform workflow and Linux Lavapipe job are configured, and Windows visible validation passes; green CI run identity, Linux artifact inspection, and formal macOS visible evidence are pending. |
| FR-019 through FR-021 | Complete | Memory/live-object gates, full local regressions, and the hardcoded-triangle scope boundary are covered. |
| SC-001 | Partial | Windows first presentation passes at 1,162.517 milliseconds with matching local screenshot/log validation; formal macOS timing evidence remains pending. |
| SC-002, SC-004 through SC-006, SC-008, SC-009 | Complete for local applicable paths | Deterministic endurance/failure tests pass; native macOS offscreen endurance also passes. |
| SC-003 | Partial | Twenty-cycle deterministic timing boundaries pass, and all 20 Windows visible recoveries pass with a 58.457-millisecond maximum; formal macOS recovery evidence is pending. |
| SC-007 | Pending | Requires a green identified CI matrix, Linux Lavapipe artifact, and both visible platform evidence pairs. |

## External Completion Gates

- GitHub Actions run/commit identity and Linux Lavapipe artifact have not yet been inspected.
- `Validation/018/macOS/triangle.png` and matching `triangle.log` do not yet exist.
- Windows run `windows-018-682e236-20260721T112047` has a matching local `Validation/018/Windows/triangle.png` and normalized `triangle.log`; the screenshot is treated as temporary local validation evidence rather than a long-term archival artifact.
- The real macOS 20-cycle resize/minimize/restore validation remains incomplete.
- The visible draw must still be routed through Renderer preparation and backend-neutral RHI execution bindings instead of direct backend command recording.
