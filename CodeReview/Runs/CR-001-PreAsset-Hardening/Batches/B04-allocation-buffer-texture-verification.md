# B04-S09: Allocation, Buffer, And Texture Verification

## Verification Target

This packet independently verifies the repairs committed at `f1a3329`:

- `CR001-B04-F007`: checked allocation accounting and budget integrity;
- `CR001-B04-F008`: unique allocator-bound release authority;
- `CR001-B04-F009`: exact checked texture footprints;
- `CR001-B04-F010`: closed wrapper construction, factory rollback, and
  bounded host mirror growth.

No production source or maintained test implementation changed during this
verification packet.

## Exact-Parent Reproduction

Exact parent `cf74523` was checked out as a temporary detached worktree, built
with strict ASan/UBSan and `graphics=disabled`, and linked with the retained
B04-S07 inspection probe. It reproduced every recorded defect signal:

```text
allocation_counter_overflow=1
overflow_budget_bypass=1
oversized_upload_throws=1
duplicate_release_accepted=1
duplicate_release_budget_bypass=1
multisample_footprint_undercounted=1
wide_format_footprint_undercounted=1
mip_chain_footprint_overcounted=1
failed_allocation_buffer_usable=1
failed_allocation_texture_valid=1
classification=allocation-buffer-texture-contract-defects
```

The process exited zero because the retained probe is a defect reproducer.
ASan and UBSan reported no error. The temporary worktree and generated parent
build were removed after evidence capture.

## Independent Current-Head Verifier

The retained B04-S09 verifier was written independently from the fix probe and
uses only current public APIs. It adds coverage beyond maintained tests:

- compile-time closure of allocation copying, allocator copying/moving, wrapper
  construction, and allocation move assignment;
- move-source inertness and exact one-ticket release authority;
- allocator destruction and reconstruction at the same address, proving the
  process identity blocks ABA release;
- byte/count failure atomicity and budget enforcement after `UINT64_MAX`;
- exact byte width for all ten current non-Unknown RHI formats;
- 1D, 2D, 3D, cube, array, cube-array, mip, and eight-sample footprints;
- addition overflow where mip zero is representable but the mip sum is not;
- allocation failure injection at concrete wrapper allocation, shared-pointer
  control-block allocation, and device tracking-vector allocation for both
  buffers and textures;
- host mirror allocation failure preserving earlier bytes and allowing a later
  successful upload;
- extreme logical buffer sparse upload and zero shutdown accounting.

The verifier was linked against the final B04-S09 ASan/UBSan strict Debug
libraries and produced:

```text
ownership_api_closed=1
move_transfers_release_authority=1
allocator_address_reuse_rejected=1
counter_overflow_atomic=1
post_overflow_budget_closed=1
allocation_count_failure_atomic=1
all_rhi_format_widths_exact=1
dimension_mip_layer_sample_matrix_exact=1
mip_sum_overflow_rejected=1
buffer_wrapper_allocation_rollback=1
buffer_control_block_rollback=1
buffer_tracking_rollback=1
texture_wrapper_allocation_rollback=1
texture_control_block_rollback=1
texture_tracking_rollback=1
host_upload_failure_preserves_prior_bytes=1
extreme_sparse_upload_and_shutdown=1
classification=allocation-buffer-texture-fixes-independently-verified
```

The process exited zero. ASan and UBSan reported no error.

## Negative API Closure

Compiling the original defect probe against current headers fails as required:

- allocation copy construction is deleted;
- allocation failure fabrication and fields are private;
- direct buffer/texture construction no longer matches public access and
  ownership arguments.

The expected-failure syntax check exited one with six compile errors. This
proves the original duplicate-release and partial-wrapper mechanisms are not
merely discouraged; they are absent from the current public API.

## Source And Call-Graph Audit

The independent eight-file source audit found no contradictory path:

- allocator validation and arithmetic complete before counter mutation;
- failed release gates precede `MarkReleased` and counter decrement;
- allocation tickets contain stable allocator pointer, process identity, reset
  epoch, and allocation ID without secondary heap storage;
- exact footprint calculation checks every multiply and mip-total addition;
- wrappers take ownership only after kind and byte-size compatibility checks;
- device factory failures release either the still-local ticket or the wrapper-
  owned ticket, depending on the failure point;
- upload range arithmetic is bounded before mirror growth and storage failures
  preserve prior vector contents;
- shutdown invalidates textures and buffers, reducing accounting to zero.

After `codegraph sync`, the index is current at 366 files, 4,962 nodes, and
15,070 edges. Production release callers are the buffer/texture invalidators
and device factory rollback paths. Checked footprint callers are allocator
creation, compatibility validation, the legacy scalar estimator, maintained
tests, and retained probes. No hidden production upload caller was introduced.

## Fresh Gate Evidence

The current source state passed:

- `fallback-strict` at `2026-07-26T14:11:02+00:00`: strict
  graphics-disabled Debug build and complete maintained tests;
- `strict-debug` at `2026-07-26T14:11:36+00:00`: strict real-graphics Debug
  build;
- `strict-release` at `2026-07-26T14:11:54+00:00`: strict real-graphics
  Release build;
- `sanitizers` at `2026-07-26T14:12:54+00:00`: strict ASan/UBSan Debug build
  and complete maintained tests.

The sanitizer run executed and passed native Vulkan instance/device, offscreen
triangle, frame-local release, and zero-live-object tests. Its optional
deferred-native skip belongs to accepted `CR001-B08-F001`; this packet neither
waives nor verifies that finding.

## Finding Decisions

- `CR001-B04-F007`: Verified.
- `CR001-B04-F008`: Verified.
- `CR001-B04-F009`: Verified.
- `CR001-B04-F010`: Verified.

No new finding was opened. No push or GitHub Actions run occurred in this
packet.
