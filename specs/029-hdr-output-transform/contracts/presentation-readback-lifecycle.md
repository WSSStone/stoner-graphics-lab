# Contract: Presentation, Readback, and Lifecycle

## Surface Capability Query

Capabilities are queried from the presentation surface and bind an exact
generation. They contain unique supported format/color-space pairs, metadata
support, extended-range availability, and any native reference-white/headroom
needed for encoding. A display move or mode change invalidates the snapshot.

## Resolved Configuration

The swapchain request contains extent, frames in flight, preferred exact format,
color space, and optional metadata. The backend returns actual extent, format,
color space, metadata digest, native encoding, reference-white/peak semantics,
and generation. Any mismatch is `Unsupported` or `ReconfigureRequired`, never a
silent fallback.

## Same-Frame Sequence

1. Acquire one image for the current resolved mode generation.
2. Validate producer handoff, output plan, compiled graph, and bindings.
3. Execute producer SceneColor and every output stage in compiled order.
4. When requested, transition exact FinalOutput to CopySource and issue the
   readback copy without flip, crop, scale, alignment, or resampling.
5. Transition the same FinalOutput/acquired image to Present.
6. Submit one ordered command sequence with one frame token and synchronization
   chain.
7. Observe completion and map/validate requested readback.
8. Present the acquired image with the same frame token/generation.
9. Publish only after every requested terminal result succeeds.

Row-pitch unpacking, declared channel swizzle, origin normalization, and inverse
transfer decoding are representation operations, not spatial transforms. They
must be predetermined from metadata and never content-searched.

## Resize and Mode Change

- Nonzero extent, output profile, color-space, format, metadata, display, or EDR
  reference/headroom change creates a new monotonic mode generation.
- Old graph resources, pipelines, render passes, framebuffers, swapchain images,
  drawable ownership, and readback buffers become stale and cannot submit.
- Zero/minimized returns `Paused` without a zero-size formal image.
- Restore requires a new exact configuration; the first successful frame uses
  only restored resources.
- Reconfiguration is transactional. Partial new resources roll back and old
  acquired resources are released once.

## Failure and Teardown

Failure injection covers allocation, capability query, mode resolution, shader/
pipeline, graph binding, drawable acquire, metadata, record, submit, completion,
copy/map, present, display move, device loss, and teardown. Dependent work stops
at the first failure. No partial output is accepted. One hundred repeated
resize/minimize/restore/profile transitions end with zero frame/output owners,
no stale token/handle success, and the declared terminal baseline.

## Native Responsibility

Vulkan/Metal adapters may map the RHI request to native pairs, set metadata,
acquire/present, copy/read back, and report actual state. They may not select a
different transform, alter pixels in backend shaders, flip orientation, or infer
acceptance. Vulkan may apply HDR10 static metadata through
`VK_EXT_hdr_metadata` when supported. Metal PQ must use `BGR10A2Unorm`, ITU-R
2100 PQ, EDR opt-in, and `EDRMetadata=nil`; it may report Core Animation color
management after Renderer-owned Rec.2020/ST-2084 encoding, but may not request
system tone mapping. Metal EDR must also set `EDRMetadata=nil`.
