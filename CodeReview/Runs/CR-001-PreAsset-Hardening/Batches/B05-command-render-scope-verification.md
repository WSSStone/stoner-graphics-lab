# B05-S03: Command Pools, Buffers, Barriers, And Render Scope Verification

## Verification Target

This packet independently verifies the repairs committed at `7e92de1`:

- `CR001-B05-F001`: retained command-buffer diagnostics lifetime;
- `CR001-B05-F002`: command/render construction and lifecycle authority;
- `CR001-B05-F003`: selected-mip, compatibility, and readback-footprint
  validation.

No production source or maintained test implementation changed during this
verification packet.

## Authority

Feature 011 FR-001 through FR-006, FR-009, FR-011, FR-013, FR-016, FR-017,
FR-020, FR-022, SC-002, and SC-003 require device-mediated bounded command
allocation, owner-controlled lifecycle transitions, valid transfer
resources/regions/ranges, explicit factory failure, shutdown invalidation,
and deterministic positive and negative coverage.

The specification does not require cross-format texture copy. Requiring equal
formats is therefore a conservative valid-resource rule rather than a scope
reduction.

## Parent And Current Comparison

The exact implementation parent `bd7b8c8` was reviewed from Git without
executing historical code. It contains each accepted defect directly:

- command-buffer construction and submission/completion transitions are
  public, and invalidation retains the device diagnostics pointer;
- device destruction does not enforce shutdown;
- texture-region checks compare nonzero mips with base-level dimensions;
- texture copy has no format/sample compatibility check;
- readback size uses duplicated format metadata and unchecked
  `row * image * depth * bytes` multiplication.

Current-source review confirms the corresponding paths are absent and the
replacement owner/validation paths are reachable from the maintained device
and command interfaces.

## Finding Verification

### CR001-B05-F001

`FVulkanDevice` performs shutdown on destruction, device cleanup invalidates
command pools, each pool invalidates its retained command buffers, and command
invalidation clears the diagnostics observer. The maintained test retains a
command buffer beyond a device scope and receives `InvalidState` on `Begin`.

### CR001-B05-F002

Command-pool, command-buffer, command-submission, render-pass, and framebuffer
construction is private to the responsible device, pool, or queue.
Submission/completion transitions are likewise private. Maintained
`static_assert` checks compile in strict Debug and Release.

Factory control-flow was reviewed for wrapper and tracking allocation
failure. Each path returns `Unavailable`; local ownership or explicit
invalidation prevents publication of a partial object. Zero configured
command capacity is also covered as an unavailable allocation.

### CR001-B05-F003

`IsRHITextureRegionValid` derives every axis from the selected mip.
Texture-to-texture recording validates lifecycle, usage, dimension, equal
format, and single-sample compatibility. Readback validation checks the
destination before usage access and uses
`TryGetRHITextureBufferCopyByteSize`, whose additions and multiplications are
bounded before the final buffer-range check.

Maintained tests reject an 8x8 region at mip 1 of an 8x8 texture, accept the
valid 4x4 region, reject incompatible formats, compute a padded 72-byte
footprint, reject overflow with zero output, and preserve the deferred
invalid-readback cleanup path.

## Local Gate Evidence

Detailed outputs are stored in the generated gate JSON files and summarized
in `Evidence/b05-command-render-scope-verification.md`.
