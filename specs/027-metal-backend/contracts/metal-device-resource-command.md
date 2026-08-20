# Contract: Metal Device, Resource, Command, And Synchronization

## Device Creation

1. Enumerate Metal devices and construct stable candidate records.
2. Honor an explicit registry ID before default ranking.
3. Validate baseline capabilities and every capability invariant.
4. Create logical command queues and owner state privately.
5. Publish an RHI device only after all mandatory state is `Ready`.

Failure at any step unwinds acquired ownership once and returns a stable result
and diagnostic. Candidate order from the native API is never authoritative.

## Resource Mapping

- Buffer/texture usage and visibility select shared, managed, or private storage.
- Managed CPU writes call the required native modification notification; managed
  GPU writes are synchronized before CPU readback.
- Private resources use bounded staging and blit operations.
- Texture row/image alignment is computed with checked arithmetic; readback
  reports or normalizes native row padding without changing logical contents.
- All RHI formats/usages are mapped explicitly. Unsupported combinations fail
  capability validation before allocation.
- Resources pending destruction remain retained by all recorded/in-flight work.

## Descriptor And Binding Mapping

- Direct binding policy `metal-direct-binding-v1` is the sole Feature 027 map.
- Tools alone assigns slots during offline MSL derivation; Asset stores the
  canonical entries and digest, Renderer transfers an immutable RHI value, and
  runtime descriptor encoding validates and consumes that exact value.
- Mapping is deterministic by shader stage, set, binding, type, and array index.
- Stage visibility, descriptor type/count, dynamic range, native index limit,
  and pipeline-layout compatibility are checked before encoding.
- Argument buffers are not used in Feature 027.
- Metal has no fallback slot allocator; absent, altered, duplicate, noncanonical,
  or capability-incompatible RHI binding metadata fails pipeline creation.

## Pipelines

Graphics pipeline validation covers native library/entry points, shader interface,
vertex input, raster/depth/blend state, attachment formats, sample count, and
binding layout. Compute validation covers native library/entry point, interface,
binding layout, and reported dispatch limits. Reuse keys include all semantic
state plus shader payload and binding-policy digests.

## Command Recording

The backend implements every applicable current `IRHICommandBuffer` operation:
begin/end, render scope, graphics/compute pipeline binding, descriptors, vertex/
index binding, viewport/scissor, draw/indexed draw/dispatch, buffer/texture copy,
texture-to-buffer readback, layout/dependency barriers, and render-pass end.

Recording validates state and stores immutable intent. Submission encodes legal
Metal render, compute, or blit encoder scopes. A required scope change ends the
current encoder before creating the next one. No failed command list is submitted.

## Queues And Synchronization

- Graphics, compute, and transfer have logical queues backed by compatible
  native command queues; presentation uses graphics submission ordering.
- Waits are encoded before dependent work and signals after producing work.
- Monotonic shared-event values and completion handlers implement cross-queue
  visibility and CPU-observable fence/semaphore completion.
- Reset creates a new logical epoch; an old completion cannot signal that epoch.
- CPU waits are bounded and block on a condition/event, never spin.
- Shutdown rejects new submission, drains accepted work, releases retained
  objects, and reaches zero in-flight count before device invalidation.

## Failure Injection And Inspection

Injection points cover device/queue creation, each resource family, shader and
pipeline creation, command encoding/commit/completion, synchronization, and
shutdown. Inspection exposes stable counts and IDs but never native pointer
values. Every injected path must end with no published partial object and zero
unexpected ownership.
