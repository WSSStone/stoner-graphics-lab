# B05-S02: Command Pools, Buffers, Barriers, And Render Scope Fix

## Repair Target

Implementation commit `7e92de1` repairs:

- `CR001-B05-F001`: command buffers retained device-owned diagnostics beyond
  the device lifetime;
- `CR001-B05-F002`: command and render wrappers bypassed owner validation and
  state authority;
- `CR001-B05-F003`: texture transfers accepted invalid mip regions,
  incompatible resources, and unchecked readback footprints.

The change remains inside RHI transfer helpers, the deterministic Vulkan
command/render ownership path, and maintained backend tests. It does not
change native Vulkan execution, shader/pipeline behavior, Renderer policy, or
Feature 020 Asset code.

## Lifetime And Ownership

`FVulkanDevice` now shuts down from its destructor when still initialized.
Command pools are the sole device-side owners and invalidators of allocated
command buffers; the redundant device command-buffer array is removed.
Invalidation clears the command buffer's diagnostics observer, so a retained
shared command buffer remains queryable and rejects later recording after its
device has left scope.

Command-pool, command-buffer, submission, render-pass, and framebuffer
constructors are private to their responsible owners. Submission/completion
state transitions are private to pool, queue, submission, and device
collaborators. Backend callers can no longer manufacture valid-looking
objects or advance command lifecycle state outside those owners.

## Explicit Factory Failure

Command-pool allocation, command-buffer allocation/tracking, submission
allocation/tracking, render-pass creation/tracking, and framebuffer
creation/tracking now catch allocation and container-capacity failures and
return `ERHIResult::Unavailable`.

Tracking failure invalidates the temporary object before it is discarded.
Submission tracking is established before synchronization or command-buffer
state changes begin. A configured command-buffer capacity of zero now means
zero available command buffers instead of silently disabling the limit.

The broader queue synchronization transaction is intentionally left to
B05-S04 through B05-S06, whose responsibility domain covers queue preflight,
wait/signal state, completion, and rollback.

## Transfer Validation

`IsRHITextureRegionValid` centralizes selected-mip and array-layer bounds.
Command texture copies now require equal dimensions and formats and require
single-sample source and destination resources.

`TryGetRHITextureBufferCopyByteSize` computes the exact final byte footprint
for tightly packed or padded rows and image slices. Every addition and
multiplication is checked; unsupported formats or unrepresentable footprints
fail without producing a size. Texture-to-buffer recording also validates the
destination resource before reading its usage, preserving deterministic
rejection for a missing readback target.

## Maintained Coverage

`Tests/VulkanBackendTests.cpp` now proves:

- direct construction and public state transitions remain closed at compile
  time;
- device destruction invalidates a retained command buffer;
- zero command-buffer capacity rejects allocation;
- selected-mip copies reject base-level-sized regions and accept the valid
  mip extent;
- incompatible texture formats reject copy recording;
- padded readback footprints produce the exact byte count and reject
  arithmetic overflow.

The existing deferred readback negative test additionally proves that a null
destination buffer is rejected and recording is reset without terminating the
test process.

## Local Verification

Fresh local results are recorded in
`Evidence/b05-command-render-scope-fix.md`:

- strict Debug build: passed with project warnings treated as errors;
- full Debug test executable: passed;
- strict Release build: passed with project warnings treated as errors;
- `git diff --check`: passed;
- temporary focused runner and progress output: removed.

## Finding State

- `CR001-B05-F001`: Fixed at `7e92de1`.
