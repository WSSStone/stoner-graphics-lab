# B05-S15 Evidence: Native Context Execution Verification

Finding `CR001-B05-F013` is verified at HEAD `9cacd5e` against fix commit
`d5b83ac`.

Evidence summary:

- Parent `d5b83ac^` returned `ResizeRequired` immediately after
  `VK_SUBOPTIMAL_KHR` acquire and cleared only `bFrameAcquired` on record or
  submit failure.
- Current HEAD has `bAcquiredSuboptimal`, `AbandonAcquiredVisibleFrame()`,
  `RecreateVisibleFenceSignaled()`, suboptimal-after-cleanup result mapping,
  and a regression report for `AcquireSuboptimal`, `Record`, and
  `SubmitAfterFenceReset`.
- Regression test observed:
  `[PASS] Vulkan visible frame failure lifecycle releases acquired state and reusable fences`

Fresh gate evidence:

- `gate-strict-debug.json`: passed at `2026-07-27T05:49:40+00:00`.
- `gate-fallback-strict.json`: passed at `2026-07-27T05:50:03+00:00`.
- `gate-strict-release.json`: passed at `2026-07-27T05:50:04+00:00`.
- `gate-sanitizers.json`: passed at `2026-07-27T05:53:09+00:00`.
- `gate-tests.json`: refreshed at `2026-07-27T05:54:10+00:00`; build passed,
  executable failed under the current Mac execution environment. A narrow scan
  reported MoltenVK `VK_ERROR_INCOMPATIBLE_DRIVER` / Metal unavailable and
  `[FAIL] Triangle demo deterministic lifecycle completes exact frame budget and shutdown`.

Decision: mark `CR001-B05-F013` Verified. The remaining tests boundary is not
evidence against the visible frame failure lifecycle fix.
