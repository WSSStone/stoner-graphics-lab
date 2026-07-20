# Research: Triangle Demo Integration Milestone

## Decision: Separate deterministic and native runtime modes explicitly

**Decision**: Introduce explicit deterministic and native runtime modes at Application/RHI/backend boundaries. Existing tests continue to request deterministic mode. `StonerDemo` visible mode requests native mode and treats unavailable native execution as failure; it never silently falls back.

**Rationale**: Existing Vulkan classes currently model valid lifecycle behavior without owning native Vulkan objects. Preserving this mode keeps fast, portable negative-path tests, while an explicit native mode prevents simulated success from being mistaken for the milestone's visible result.

**Alternatives considered**:

- Infer runtime mode only from whether SDK headers were found: rejected because compile-time availability does not prove a loader, ICD, display, or successful native object creation.
- Replace deterministic behavior entirely with native Vulkan: rejected because CI and failure injection rely on deterministic execution.
- Allow visible mode to fall back with a warning: rejected by FR-003 and the clarification that actual presentation must remain distinguishable.

## Decision: Complete the GLFW-first private window strategy

**Decision**: Implement the existing private GLFW driver as the first real desktop adapter. It owns `GLFWwindow`, translates callbacks to existing window/input events, provides framebuffer pixel extent, and exposes an opaque `FPlatformWindow` bridge for surface creation. Public Application headers do not expose GLFW types.

**Rationale**: GLFW 3.4 supports Windows, macOS, Linux, Vulkan surface creation, required-instance-extension discovery, and runtime Vulkan availability checks. Its Vulkan guide requires using the extension list returned by GLFW for window-surface instances and documents MoltenVK portability requirements on macOS ([GLFW Vulkan guide](https://www.glfw.org/docs/latest/vulkan_guide.html)).

**Alternatives considered**:

- Implement Win32 and Cocoa adapters now: rejected because the roadmap selected GLFW-first and native adapters are later work.
- Put GLFW calls in `StonerDemo`: rejected because it would bypass the Application window boundary.
- Keep `FWindow` as state-only and create a second unrelated real-window object: rejected because duplicated lifecycle state would drift.

## Decision: Add native Vulkan ownership behind current backend classes

**Decision**: Extend `FVulkanInstance`, `FVulkanDevice`, surface, swapchain, resources, shaders, pipelines, command buffers, queues, fences, and semaphores with native ownership guarded by the selected runtime mode. Shared native instance/device lifetime is represented by a backend-private context so child wrappers cannot outlive required parents. Existing deterministic fields and failure injection remain intact.

**Rationale**: The current classes already express the correct RHI object categories and invalidation rules, so incrementally adding native handles preserves contracts and tests. Native objects must be destroyed in reverse dependency order after device idle; runtime snapshots expose stable counts rather than native addresses.

**Alternatives considered**:

- Add a separate parallel `RealVulkan*` class hierarchy: rejected because it duplicates all RHI behavior and creates two backends with diverging semantics.
- Put native handles into public RHI interfaces: rejected because it violates multi-API support.
- Implement raw Vulkan only in the demo: rejected because Renderer/Application must not bypass RHI.

## Decision: Extend RHI only for missing presentation and triangle command capabilities

**Decision**: Add backend-neutral presentation surface/swapchain descriptions, swapchain image access as imported `IRHITexture` objects, acquire/present synchronization parameters, buffer upload, vertex-buffer binding, viewport/scissor state, clear values, and explicit runtime-mode inspection. Keep backend-specific queue families, image layouts, native handles, and extension names private.

**Rationale**: The current contracts can validate symbolic draw and render-pass order but cannot bind a vertex buffer, access a presentable image, carry acquire/present synchronization, or prove native execution. These are the minimum missing capabilities for FR-006 through FR-010.

**Alternatives considered**:

- Downcast RHI objects to Vulkan types in Renderer: rejected because it bypasses abstraction.
- Add a broad modern RHI redesign: rejected because descriptors, textures, compute, and advanced pipeline features beyond the triangle are already separate phases.
- Use shader-generated vertices and avoid a vertex buffer: rejected because the roadmap and FR-006 explicitly require vertex data upload.

## Decision: Translate forward plans through a dedicated Renderer RHI executor

**Decision**: Add `FForwardFrameExecutor` in Renderer. It consumes a valid `FForwardFramePlan`, imported RHI output, pipeline/resource bindings, and one frame context; it builds/executes the render graph, emits each transition immediately before the pass that requires it, records the triangle draw, and returns a normalized execution result.

**Rationale**: The current render-graph executor emits all symbolic transitions before callbacks and explicitly notes that real commands must interleave transitions per pass. A dedicated executor keeps planning and execution separate and gives future backends one RHI-level path.

**Alternatives considered**:

- Make `FForwardRenderer::PrepareFrame` record GPU commands: rejected because frame validation/planning and execution have different lifecycles and failure modes.
- Record commands directly from `StonerDemo`: rejected because it would skip Renderer/render-graph integration.
- Generalize every render-graph resource allocator in this milestone: rejected because one imported color output and bounded transient support are sufficient.

## Decision: Default to two frames in flight and key present completion by swapchain image

**Decision**: Default `MaxFramesInFlight` to 2 and clamp it to runtime capabilities. Each frame owns an acquire semaphore, command buffer, and submit fence. Render-finished semaphores are allocated per swapchain image and selected by the acquired image index.

**Rationale**: Khronos' triangle guidance uses two frames to overlap host and device without allowing the host to get far ahead, with per-frame command buffers, semaphores, and fences ([Frames in flight](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html)). Khronos also warns that submit-fence completion does not prove presentation has finished with a semaphore and recommends present-wait semaphore reuse keyed by swapchain image ([Swapchain semaphore reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)).

**Alternatives considered**:

- One frame in flight: correct but unnecessarily serial and less representative of real engine execution.
- Three or more by default: rejected for the first triangle because it increases synchronization state and latency without milestone value.
- One render-finished semaphore per frame: rejected because presentation lifetime is tied to swapchain images, not only submit fences.

## Decision: Use correctness-first swapchain recreation

**Decision**: On non-zero resize or out-of-date/suboptimal presentation, stop new frame submission, wait for device/graphics queue idle, destroy framebuffer/image-view/swapchain-dependent objects, recreate from current framebuffer extent, and resume. At zero extent, continue event polling and wait for a non-zero extent without presenting.

**Rationale**: This simple path is correct for a one-window demo and meets the two-second resume target. Khronos documents that resize invalidates swapchain compatibility and that minimization produces zero framebuffer size; swapchain-dependent resources must be rebuilt only after a valid extent returns ([Swapchain recreation](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/04_Swap_chain_recreation.html)). More advanced deferred retirement is valuable later but adds substantial present-history complexity ([Swapchain recreation sample](https://docs.vulkan.org/samples/latest/samples/api/swapchain_recreation/README.html)).

**Alternatives considered**:

- Continue using the old swapchain through resize: rejected as invalid.
- Implement deferred old-swapchain retirement now: rejected as unnecessary complexity for a single triangle.
- Block the entire event loop while minimized: rejected because close/restore input must remain responsive.

## Decision: Use source-controlled GLSL and offline SPIR-V payloads

**Decision**: Keep minimal GLSL vertex/fragment sources and checked-in `.spv` outputs. SCons recompiles them with an offline compiler when available and can verify committed outputs; otherwise it validates and copies the checked-in payloads. Runtime shader compilation is not added.

**Rationale**: Vulkan consumes SPIR-V shader code, while offline compilation keeps runtime behavior small and reproducible ([Vulkan shader modules](https://docs.vulkan.org/spec/latest/chapters/shaders.html), [Vulkan sample shader workflow](https://docs.vulkan.org/samples/latest/shaders/README.html)). Checked-in payloads let headless CI build the demo without assuming an SDK compiler on every runner.

**Alternatives considered**:

- Runtime GLSL compilation: rejected by scope and introduces a compiler library dependency.
- Commit only binary SPIR-V: rejected because source review and reproducibility would be poor.
- Require a shader compiler on every build: rejected because deterministic/headless compilation should remain available when native dependencies are absent.

## Decision: Run Linux native integration with Mesa Lavapipe and no surface

**Decision**: The Linux CI native job installs the Vulkan loader, development headers, Mesa Vulkan drivers, and tools; selects the Lavapipe ICD through `VK_DRIVER_FILES`; creates a native instance/device, offscreen color target, real buffers/shaders/pipeline/commands, submits through the graphics queue, waits for completion, and shuts down without creating a window or swapchain.

**Rationale**: Mesa documents the Lavapipe ICD filename (`lvp_icd.x86_64.json`) and selecting an ICD through `VK_DRIVER_FILES` ([Mesa installation](https://docs.mesa3d.org/install.html), [Mesa environment variables](https://docs.mesa3d.org/envvars.html)). This exercises real Vulkan code on a CPU device without claiming Linux visible presentation.

**Alternatives considered**:

- Deterministic fallback only on Linux: rejected by clarification Q4.
- Xvfb plus Lavapipe presentation: not required because the user lacks Linux graphics validation and no-window native execution is the agreed scope.
- Require a physical GPU runner: rejected because hosted CI cannot assume one.

## Decision: Use tiered bounded endurance profiles and two leak signals

**Decision**: All values remain CLI-configurable. Defaults are:

| Profile | Frames | Warm-up | Sample interval | Max steady RSS growth |
|---------|-------:|--------:|----------------:|----------------------:|
| Deterministic CI | 4,096 | 512 | 128 | max(16 MiB, 5% of baseline) |
| Linux software Vulkan CI | 4,096 | 512 | 128 | max(64 MiB, 10% of baseline) |
| Windows/macOS real smoke | 10,000 | 1,000 | 120 | max(64 MiB, 10% of baseline) |

The baseline is the median of the first five samples after warm-up; final steady usage is the median of the final five samples. Success requires final median minus baseline median to remain within the configured limit, live runtime counts to remain bounded during the run, and all demo-owned live counts to reach zero after shutdown.

**Rationale**: A frame count alone cannot prove absence of leaks. Warm-up excludes expected loader/pipeline/cache initialization, medians reduce scheduler/allocator noise, and separate thresholds acknowledge software-driver/native-runtime variance. LeakSanitizer remains a supplemental Linux/macOS diagnostic because official Clang documentation notes supported leak detection there, while it is not a uniform Windows gate ([LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html), [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)). Defaults intentionally exceed 300 frames and can be raised for scheduled/manual endurance runs.

**Alternatives considered**:

- Exactly 300 frames with no memory sampling: rejected because it is insensitive to slow growth.
- Require zero RSS change: rejected because graphics loaders, allocators, and drivers have legitimate lazy allocation and noisy resident-set accounting.
- Make platform leak tools the only gate: rejected because no one tool provides equivalent behavior across Windows, macOS, and Linux.

## Decision: Real presentation evidence is manual and retained

**Decision**: Windows and macOS validation each retain one screenshot and its matching normalized successful run log under `Validation/018/<Platform>/`. The log records native runtime mode, adapter identity without native addresses, frame budget/completed frames, resize/recovery count, memory summary, resource-count summary, and clean shutdown. A human confirms triangle shape and distinguishable RGB interpolation. Automated golden-image comparison is deferred.

**Rationale**: This meets the clarified evidence requirement without expanding the milestone into platform screenshot capture, readback, color-management normalization, or golden-image tolerance design.

**Alternatives considered**:

- Present-result logs only: rejected because they do not prove visible pixels.
- Automated coarse pixel readback: deferred because it requires extra transfer/readback contracts.
- Pixel-perfect golden images: rejected because driver, scale, color, and presentation differences make a first-milestone golden test brittle.
