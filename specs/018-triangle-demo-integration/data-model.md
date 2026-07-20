# Data Model: Triangle Demo Integration Milestone

## Overview

Feature 018 owns no persistent scene data. Its data model describes one demo process, one optional real window/presentation chain, one triangle resource set, bounded frame contexts, deterministic diagnostics, and one validation report. Native graphics handles remain private to the Vulkan backend and never appear in public model fields or dumps.

## Enumerations

### `EDemoRunMode`

- `InteractiveNative`: real window and real Vulkan presentation; runs until close or Escape.
- `BoundedNative`: real window and real Vulkan presentation; exits after the configured drawable-frame budget.
- `DeterministicHeadless`: no native window or graphics runtime; validates orchestration and injected failures.
- `NativeHeadless`: no window or swapchain; executes real Vulkan against an offscreen color target, primarily through Lavapipe in Linux CI.

### `EDemoLifecycleState`

- `Uninitialized`
- `InitializingWindow`
- `InitializingRuntime`
- `InitializingResources`
- `Ready`
- `Running`
- `PresentationPaused`
- `RecreatingPresentation`
- `Stopping`
- `Stopped`
- `Failed`

### `EDemoFrameStage`

- `PollEvents`
- `CheckDrawable`
- `WaitFrameContext`
- `AcquireOutput`
- `PrepareFrame`
- `RecordCommands`
- `Submit`
- `Present`
- `CollectValidationSample`
- `CompleteFrame`

### `EDemoResult`

- `Success`
- `InvalidConfiguration`
- `DependencyUnavailable`
- `NativeRuntimeUnavailable`
- `InitializationFailed`
- `FrameFailed`
- `ResizeRequired`
- `PresentationPaused`
- `ValidationFailed`
- `ShutdownFailed`

## Entity: Demo Configuration

Represents immutable launch policy after command-line parsing.

| Field | Type | Default | Validation |
|-------|------|---------|------------|
| `RunMode` | `EDemoRunMode` | `InteractiveNative` | Must be one canonical mode |
| `ClientWidth` | positive integer | `1280` | 1..16384 for visible modes |
| `ClientHeight` | positive integer | `720` | 1..16384 for visible modes |
| `FrameBudget` | non-negative integer | mode profile | Must be positive for bounded/headless validation; ignored for interactive mode |
| `WarmupFrames` | non-negative integer | mode profile | Must be less than `FrameBudget` |
| `MemorySampleInterval` | positive integer | mode profile | Must permit at least ten post-warm-up samples |
| `MaxMemoryGrowthBytes` | positive integer | mode profile | Explicit override or derived mode default |
| `MaxMemoryGrowthPercent` | positive percentage | mode profile | Explicit override or derived mode default; combined with absolute floor |
| `MaxFramesInFlight` | positive integer | `2` | Clamped to runtime/swapchain capabilities |
| `bEnableValidationLayers` | boolean | debug=true | Native runtime may report unavailable layers without claiming they were enabled |
| `ShaderDirectory` | path | build-relative demo shader path | Must contain valid vertex and fragment SPIR-V payloads |
| `ValidationOutputPath` | path | build-relative log path | Parent must be writable for bounded validation |

Relationships:

- One configuration creates exactly one `Demo Application`.
- One configuration selects one endurance profile and one runtime strategy.

## Entity: Demo Application

Coordinates ownership but delegates work to window, Renderer executor, RHI device, validation monitor, and diagnostics.

| Field | Type | Rule |
|-------|------|------|
| `Configuration` | Demo Configuration | Immutable after initialization starts |
| `LifecycleState` | `EDemoLifecycleState` | Follows the state machine below |
| `Window` | optional Application window | Present only for native visible modes |
| `RuntimeModeProof` | RHI runtime mode summary | Native modes require native proof |
| `PresentationState` | optional Presentation State | Present only for visible modes |
| `TriangleResources` | Triangle Resource Set | Created once per runtime; presentation-dependent children may be recreated |
| `FrameContexts` | ordered array | Size is clamped frames-in-flight count |
| `CurrentFrameSlot` | integer | `CompletedDrawableFrames % FrameContexts.size()` |
| `CompletedDrawableFrames` | integer | Increments only after successful submit and, for visible mode, present |
| `StartupTimestamp` | monotonic timestamp | Captured at process entry before dependency or native initialization |
| `FirstPresentTimestamp` | optional monotonic timestamp | Captured after the first successful visible present |
| `Diagnostics` | Demo Diagnostic Log | Stable by sequence and diagnostic code |
| `ValidationMonitor` | Validation Monitor | Active in bounded/headless modes |

Composite ownership rules:

- `Demo Application` is the lifecycle Composite root.
- `Presentation State`, `Triangle Resource Set`, and the `Frame Context` collection are independently owned child composites with idempotent release operations.
- The root releases child composites in the shutdown contract's reverse dependency order; child composites never destroy siblings or backend parents.

Lifecycle transitions:

```text
Uninitialized
  -> InitializingWindow        (visible modes)
  -> InitializingRuntime       (headless modes)
InitializingWindow
  -> InitializingRuntime       (real window created)
  -> Failed                    (window/dependency unavailable)
InitializingRuntime
  -> InitializingResources     (required native/deterministic mode proven)
  -> Failed                    (device/surface/queue unavailable)
InitializingResources
  -> Ready                     (triangle, shaders, pipeline, graph, frame contexts ready)
  -> Failed                    (resource creation/upload/compatibility failure)
Ready -> Running
Running
  -> PresentationPaused        (zero drawable extent)
  -> RecreatingPresentation    (resize/out-of-date/suboptimal)
  -> Stopping                  (close, Escape, or frame budget complete)
  -> Failed                    (non-recoverable frame failure)
PresentationPaused
  -> Running                   (valid extent returns and presentation is ready)
  -> Stopping                  (exit request)
RecreatingPresentation
  -> Running                   (replacement presentation state ready)
  -> PresentationPaused        (extent becomes zero)
  -> Failed                    (recreation fails)
Failed -> Stopping
Stopping -> Stopped
```

## Entity: Presentation State

Represents swapchain-dependent state; unavailable in deterministic/native headless modes.

| Field | Type | Rule |
|-------|------|------|
| `Generation` | positive integer | Increments after each successful recreation |
| `DrawableWidth` / `DrawableHeight` | integer | Both positive while ready; both zero while paused |
| `Format` | RHI format | Selected from supported surface formats |
| `PresentMode` | RHI present mode | FIFO-compatible default; selected from supported modes |
| `ImageCount` | positive integer | Runtime-supported count |
| `CurrentImageIndex` | optional integer | Valid only after successful acquire and before present/release |
| `SwapchainImages` | RHI texture array | One imported color texture per presentation image |
| `Framebuffers` | RHI framebuffer array | One per swapchain image, matching generation |
| `RenderFinishedSignals` | synchronization array | One per swapchain image |
| `State` | ready/acquired/paused/recreate-required/unavailable | Controls legal frame operations |
| `RecoveryStartTimestamp` | optional monotonic timestamp | Captured when a restored non-zero extent first requires a replacement generation |
| `RecoveryDurationsMilliseconds` | ordered integer array | One duration per successfully restored replacement generation |

Validation rules:

- Objects from an old generation cannot be used after recreation.
- Image-indexed arrays have identical length and use the acquired image index.
- Zero drawable extent has no live acquired image.
- Recreation first waits for relevant native work to become idle, then destroys old dependent objects in reverse order.

## Entity: Frame Context

Reusable per-frame-slot synchronization and command state.

| Field | Type | Rule |
|-------|------|------|
| `SlotIndex` | integer | Unique in `[0, MaxFramesInFlight)` |
| `SubmissionSerial` | integer | Monotonically increases when the slot is submitted |
| `CommandBuffer` | RHI command buffer | Reset only after its submit fence completes |
| `AcquireSignal` | RHI semaphore | Used only for visible acquisition |
| `SubmitFence` | RHI fence | Signaled initially; waited/reset before slot reuse |
| `AcquiredImageIndex` | optional integer | Cleared after present/recreation |
| `State` | idle/recording/submitted/complete/invalid | Legal transitions are strict |

Relationship:

- Frame-slot resources are indexed by `CurrentFrameSlot`.
- Presentation completion signal is selected by acquired image index, not frame slot.

## Entity: Triangle Resource Set

| Field | Type | Lifecycle |
|-------|------|-----------|
| `VertexPayload` | three position/color vertices | Immutable process data |
| `VertexBuffer` | RHI buffer | Created/uploaded once; destroyed before device |
| `VertexShader` | RHI shader module | Created from validated offline SPIR-V |
| `FragmentShader` | RHI shader module | Created from validated offline SPIR-V |
| `PipelineLayout` | RHI pipeline layout | No descriptors required for v1 triangle |
| `RenderPass` | RHI render pass | Compatible with selected color format |
| `GraphicsPipeline` | RHI graphics pipeline | Triangle list, vertex layout, no depth/blending |
| `ForwardFramePlan` | Renderer frame plan | One accepted opaque triangle draw and imported output |
| `RenderGraph` | compiled graph | Transition -> color pass -> presentation-ready/output completion |

Validation rules:

- Vertex count is exactly three and area is non-zero.
- Vertex colors contain distinguishable red, green, and blue values.
- Shader payloads are non-empty, 4-byte aligned SPIR-V and match declared stages/entry points.
- Pipeline output format matches current presentation/offscreen target.
- Presentation-format change recreates format-dependent render pass/pipeline/framebuffers.

## Entity: Runtime Snapshot

Stable backend-neutral proof used by diagnostics and validation.

| Field | Type | Meaning |
|-------|------|---------|
| `RuntimeMode` | deterministic/native | Whether native API objects are owned |
| `AdapterName` | normalized string | Human-readable adapter/software-device name |
| `bSoftwareDevice` | boolean | True for Lavapipe/software execution |
| `LiveInstanceCount` | integer | Native instances owned |
| `LiveDeviceCount` | integer | Native logical devices owned |
| `LiveSurfaceCount` | integer | Native presentation surfaces owned |
| `LiveSwapchainCount` | integer | Native swapchains owned |
| `LiveResourceCount` | integer | Buffers/images/views/memory allocations |
| `LivePipelineCount` | integer | Shader/pipeline/layout/render-pass objects |
| `LiveCommandCount` | integer | Pools/command buffers |
| `LiveSyncCount` | integer | Semaphores/fences |

No native address or integer handle value is exposed.

## Entity: Validation Monitor

| Field | Type | Rule |
|-------|------|------|
| `WarmupFrames` | integer | Samples before this completed-frame count are ignored |
| `SampleInterval` | integer | Resident memory sampled on completed-frame multiples |
| `Samples` | ordered memory samples | At least ten after warm-up |
| `BaselineMedianBytes` | integer | Median of first five post-warm-up samples |
| `FinalMedianBytes` | integer | Median of final five samples |
| `AllowedGrowthBytes` | integer | `max(configured absolute limit, configured percentage of baseline)` |
| `PeakLiveCounts` | runtime snapshot | Peak resource counts after initialization |
| `FinalLiveCounts` | runtime snapshot | Must be zero after shutdown for demo-owned categories |

Memory sample fields:

- completed drawable frame index
- monotonic timestamp
- process resident bytes
- runtime live-count snapshot

## Entity: Demo Diagnostic

| Field | Type | Rule |
|-------|------|------|
| `Sequence` | integer | Monotonic within process |
| `Severity` | info/warning/error/fatal | Stable classification |
| `Stage` | lifecycle or frame stage | Identifies first failing stage |
| `Code` | stable string | `DEMO-<CATEGORY>-<REASON>` |
| `Result` | `EDemoResult` | Normalized outcome |
| `Subject` | stable string | Resource category or frame serial, never native address |
| `Message` | string | Human-readable normalized detail |

Primary-failure rule: the first error/fatal diagnostic is the authoritative failure; downstream cleanup diagnostics cannot replace it.

## Entity: Validation Report

| Field | Type | Pass rule |
|-------|------|-----------|
| `Platform` | Windows/macOS/Linux | Matches host |
| `RunMode` | `EDemoRunMode` | Matches requested profile |
| `RuntimeModeProof` | deterministic/native | Native-required profiles must report native |
| `RequestedFrames` / `CompletedFrames` | integer | Equal for successful bounded run |
| `TimeToFirstPresentMilliseconds` | optional integer | Required for visible evidence and <= 5,000 |
| `WarmupFrames` / `SampleCount` | integer | Sample count >= 10 |
| `BaselineMedianBytes` / `FinalMedianBytes` | integer | Delta <= allowed growth |
| `PeakLiveCounts` / `FinalLiveCounts` | snapshots | Final demo-owned counts are zero |
| `ResizeRecoveryCount` | integer | Every started recovery has a completion or failure |
| `RecoveryDurationsMilliseconds` | ordered integer array | Exactly one entry per completed recovery and every entry <= 2,000 |
| `FirstFailure` | optional diagnostic | Empty on success |
| `ExitCode` | integer | Matches runtime contract |
| `NormalizedLogPath` | path | Exists for retained evidence profiles |
| `ScreenshotPath` | optional path | Required for Windows/macOS visible completion evidence |

Report ordering and textual formatting are deterministic for identical deterministic inputs.
