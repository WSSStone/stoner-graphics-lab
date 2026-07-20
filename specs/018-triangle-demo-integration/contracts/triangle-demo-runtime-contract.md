# Contract: Triangle Demo Runtime

## Purpose

Define the observable executable, Application/Renderer/RHI handoff, frame lifecycle, result, and shutdown behavior for Feature 018. This contract does not expose Vulkan or GLFW native types.

## Executable Contract

Target name: `StonerDemo`

```text
StonerDemo
  [--mode interactive|validate|headless|headless-vulkan]
  [--frames <positive integer>]
  [--warmup-frames <non-negative integer>]
  [--memory-sample-interval <positive integer>]
  [--max-memory-growth-mib <positive integer>]
  [--max-memory-growth-percent <positive number>]
  [--width <positive integer>]
  [--height <positive integer>]
  [--frames-in-flight <positive integer>]
  [--shader-dir <path>]
  [--validation-output <path>]
  [--enable-validation]
```

### Mode semantics

| CLI mode | Window | Graphics runtime | Termination |
|----------|--------|------------------|-------------|
| `interactive` | Real visible | Native Vulkan + presentation | Window close or Escape |
| `validate` | Real visible | Native Vulkan + presentation | Configured drawable-frame budget or earlier failure/exit |
| `headless` | Deterministic | Deterministic RHI/backend strategy | Configured frame budget |
| `headless-vulkan` | None | Native Vulkan + offscreen target | Configured frame budget |

Defaults:

- No `--mode`: `interactive`.
- `--frames` is required to be positive for bounded modes; profile defaults apply when omitted.
- `--warmup-frames < --frames`.
- `--frames-in-flight` defaults to 2 and is clamped to runtime capability.
- Memory growth uses the greater of the configured absolute MiB floor and configured percentage of the post-warm-up baseline.
- `interactive` ignores frame-budget options and never auto-reports validation success.

Unknown options, malformed numbers, zero bounded frame budgets, invalid extents, unwritable output paths, or warm-up not smaller than frame budget return `InvalidConfiguration` before native initialization.

## Exit Codes

| Code | Meaning |
|-----:|---------|
| 0 | Requested interactive shutdown or bounded validation completed successfully |
| 2 | Invalid command-line/configuration |
| 3 | Required dependency/native runtime unavailable |
| 4 | Window, instance, device, surface, swapchain, shader, resource, or pipeline initialization failed |
| 5 | Non-recoverable frame acquire/record/submit/present failure |
| 6 | Endurance resource-count or memory-growth validation failed |
| 7 | Required validation report/log could not be written |

The first non-success outcome owns the final exit code. Cleanup errors are logged but cannot overwrite an earlier primary failure.

## Application Window Contract

- `FWindow` owns one private `IWindowDriver` strategy.
- The GLFW strategy creates a `GLFW_NO_API` window, polls native events, and translates resize, framebuffer-size, minimize, restore, focus, key, and close callbacks into existing Application events.
- `FWindow::GetPlatformWindow()` returns an opaque Core wrapper only when a real window exists.
- `FWindow::GetDrawableWidth/Height()` uses framebuffer pixel extent, which may differ from logical client extent on high-DPI displays.
- Headless window behavior remains deterministic and does not fabricate a native handle.
- Escape maps to the existing physical-key input vocabulary and requests close.

## RHI Presentation Contract

### Presentation surface

`IRHIPresentationSurface` exposes:

- lifecycle state
- current drawable extent summary
- compatibility query for a device/queue
- invalidation

Creation consumes an opaque `FPlatformWindow`. Backend native handles remain private.

### Swapchain

`FRHISwapchainDesc` includes:

- presentation surface
- requested width/height
- preferred color formats
- preferred present mode
- requested frames in flight

`IRHISwapchain` exposes:

- selected format and extent
- image count
- imported swapchain images as `IRHITexture`
- `AcquireNextFrame(acquireSignal, outImageIndex)`
- `Present(imageIndex, renderFinishedSignal)`
- resize-required/unavailable lifecycle states

Acquire/present results normalize native success, suboptimal, out-of-date, surface-lost, device-lost, timeout, and unavailable outcomes to stable RHI results.

### Required command capabilities

The triangle path requires backend-neutral commands for:

- upload/copy into a vertex buffer
- transition imported output to color-attachment use
- begin render pass with clear color
- bind graphics pipeline
- bind one vertex buffer with offset/stride metadata
- set viewport and scissor from current output extent
- draw exactly three vertices and one instance
- end render pass
- transition visible output to presentation-ready use

Command methods reject missing, invalidated, wrong-usage, incompatible-format, or wrong-lifecycle objects before native submission.

## Renderer Execution Contract

`FForwardFrameExecutor` accepts:

- valid `FForwardFramePlan`
- RHI device/graphics queue
- imported output texture and current output extent
- compatible triangle pipeline/resource bindings
- reusable frame command buffer and synchronization context

Execution order:

1. Validate plan, imported output, resources, and runtime-mode requirement.
2. Resolve graph imported output to the current RHI texture.
3. For each scheduled pass, emit required transitions immediately before the pass.
4. Record clear, pipeline/vertex binding, viewport/scissor, and triangle draw.
5. End command recording and return an executable command buffer.

The executor never acquires or presents; those operations remain in the demo frame coordinator. Deterministic mode emits the same normalized stage records without claiming native execution.

## Native Frame Contract

For each drawable visible frame:

1. Poll window/input events.
2. Honor close/Escape before acquiring a new image.
3. If drawable extent is zero, enter `PresentationPaused`, process events, and skip acquire/submit/present.
4. Select frame slot by completed drawable-frame count modulo frame-context count.
5. Wait for and reset the slot's submit fence.
6. Acquire one swapchain image signaling the slot's acquire semaphore.
7. Reset and record the slot's command buffer through `FForwardFrameExecutor`.
8. Submit, waiting on acquire and signaling the render-finished semaphore selected by acquired image index.
9. Present the acquired image waiting on that image's render-finished semaphore.
10. Collect validation samples when due.
11. Increment completed drawable frames only after successful submission and presentation.

Native headless mode follows the same prepare/record/submit/completion order but uses an offscreen output and omits acquire/present.

## Resize and Presentation Recovery

- Zero drawable extent enters `PresentationPaused`; no swapchain is created/recreated until both dimensions are positive.
- Non-zero framebuffer-size change, acquire out-of-date, or present out-of-date/suboptimal enters `RecreatingPresentation`.
- New frame submission stops, the relevant device/queue becomes idle, and old framebuffer/image-view/swapchain-dependent objects are destroyed in reverse dependency order.
- Replacement surface-compatible format, extent, image views, framebuffers, and format-dependent render pass/pipeline are built as one new generation.
- Any acquired image from the retired generation is discarded and cannot be presented.
- Successful recreation returns to `Running`; failure becomes the primary failure and begins shutdown.
- Recovery timing starts when the loop first observes a valid non-zero drawable extent after pause/restore and ends after the first successful presentation using the replacement generation; every completed recovery must be no more than 2,000 milliseconds.

## Presentation Timing Contract

- Process startup timing begins at executable entry before dependency checks or native initialization.
- First-presentation timing ends only after the first successful visible present; initialization or command submission without presentation does not satisfy it.
- Visible validation reports `TimeToFirstPresentMilliseconds` and fails when it exceeds 5,000 milliseconds.
- Deterministic tests use an injected monotonic clock to verify exact boundary behavior without wall-clock flakiness; retained Windows/macOS logs carry the measured native values.

## Shader Contract

- Vertex and fragment GLSL sources are repository-owned.
- Matching checked-in SPIR-V payloads are required for builds without a shader compiler.
- Build-time compiler presence regenerates or verifies payloads; validator presence runs SPIR-V validation.
- Runtime requires SPIR-V magic, 4-byte alignment, non-empty payload, expected stage, `main` entry point, and pipeline interface compatibility.
- Native shader-module or pipeline creation failure cannot fall back to structural-only success in native mode.

## Shutdown Contract

Shutdown is idempotent after it starts and proceeds in this dependency order:

1. Stop acquiring/submitting frames.
2. Wait for native graphics/device idle when native initialization reached submission capability.
3. Release frame command/synchronization contexts.
4. Release framebuffers and presentation-dependent render resources.
5. Release graphics pipeline, render pass, pipeline layout, shader modules, and vertex buffer/allocation.
6. Release swapchain images/views and swapchain.
7. Release presentation surface.
8. Release queue, logical device, and instance.
9. Destroy the real window and terminate its driver ownership.
10. Capture final zero-live-resource snapshot and write the normalized validation report/log.

Every step tolerates partially initialized prior steps. Public diagnostics contain stable categories and adapter names but no native addresses.
