# Production Environment Authority Contract

## Purpose

This contract separates correctness facts from measurements whose meaning
depends on the execution environment. It does not relax deterministic, native,
lifecycle, ownership, stale-handle, capture, readback, semantic, or physical
image requirements.

## Execution Classes

| Class | Selection | Permitted authority |
|---|---|---|
| `github-hosted` | Workflow-owned hosted job definition | Build, deterministic cook, strict runtime, exact lifecycle work, native proof, captures/readbacks, ownership, stale rejection, crash, and operational timeout |
| `maintainer-local-metal` | Explicit hardware-profile flag plus successful native arm64 macOS Metal preflight | All hosted correctness facts plus calibrated RSS and required image/FLIP decisions for the exact Metal device class |
| `maintainer-local-windows-vulkan` | Explicit hardware-profile flag plus successful native x86_64 Windows Vulkan preflight | All hosted correctness facts plus required image/FLIP decisions for the exact Vulkan device class; working-set RSS remains observed |
| `local-diagnostic` | Local runner default | Reproduction and observations only; cannot satisfy required hosted/physical closeout authority |

The generic class is not accepted from an unrestricted application or runner
CLI token. `--local-metal-authority` and `--local-windows-vulkan-authority` are
narrow, mutually exclusive maintainer assertions, not class selectors. Each is
valid only for the hardware profile and its exact native host/target. Missing, conflicting, dirty-revision,
translated, allocator-overridden, nonexclusive, or other-backend authority
fails closed.

## Measurement Dispositions

| Disposition | Meaning | Result behavior |
|---|---|---|
| `required` | Calibrated hard acceptance condition | Missing or exceeded evidence fails |
| `operational` | Bound preventing an unbounded or stuck job | Fails only when required work does not complete before timeout |
| `observed` | Diagnostic/trend measurement | Always serialized when available; cannot independently pass or fail the run |
| `not-required` | Profile owns no authority for the measurement | Must carry a stable reason; cannot fabricate a measurement |

Aggregation preserves the producer disposition exactly. It cannot convert
`observed` or `not-required` into `required`, and it cannot use an observed value
to choose a pass/fail result.

## Hosted Medium Contract

Hosted Medium remains a hard functional/lifecycle gate and requires, per exact
package shard:

- clean cook plus unchanged 100-percent-reuse warm cook;
- publication, source-unavailable strict runtime, and semantic equivalence;
- requested native backend proof;
- 1,000 cycles with cycles 1-20 included as warm-up;
- exactly 2,000 Deferred/Forward captures;
- exactly seven retained post-lifecycle readbacks;
- every tracked terminal owner at baseline;
- stale-handle rejection;
- a 6,600-second complete-package operational timeout;
- a 6,900-second complete-profile operational timeout;
- an independent 6,000-second native operational timeout inside a 150-minute job.

RSS endpoints, task-VM categories, allocator totals, peak memory, and elapsed
time are `observed`. Hosted execution uses normal production allocator behavior;
validation-only allocator-zone switches cannot manufacture RSS authority.

## Maintainer-Local Physical Preflight

Before Metal RSS or either physical image evidence is `required`, the lane proves:

1. exact native OS/architecture/backend target and registered device/backend capability signature; Metal additionally proves non-Rosetta arm64 macOS;
2. exclusive process/device/display ownership, enforced by a repository-scoped nonblocking authority lock;
3. clean committed HEAD plus exact source revision, workload revision, target profile, and software image;
4. default production allocator behavior;
5. declared warm-up, queue-idle, teardown, sample, and capture protocol;
6. required application-window presentation and readback capability.

Failed preflight produces `Unsupported` with a stable missing prerequisite and
replacement command. It cannot fall back to hosted or ordinary local measurements.

The maintainer-local Metal lifecycle retains the 16 MiB post-warm-up-to-terminal
RSS hard limit. The Windows Vulkan lifecycle records the same RSS endpoints,
milestones, peak, and bounded diagnostics with `observed` disposition; its
working-set value cannot independently pass or fail the run. Required image
acceptance on both physical lanes retains exact workload/backend/device-class
baseline selection, region semantics, same-size FLIP, and explicit maintainer-
accepted reference state. Windows validation must not trim its working set,
change allocator policy, move sampling points, or inflate a threshold to create
a favorable result. A stable Windows reference-set/WPR memory gate is deferred
to future hardware-lab qualification.

## Image Registration

Semantic classification uses workload-versioned bounded regions, minimum valid-
sample coverage, and robust color/direction/depth/lighting statistics. One exact
pixel cannot be the semantic authority.

Final perceptual comparison remains spatially registered. It cannot translate,
scale, crop, warp, resample, or search for a best alignment. Calibration must
reject an intentional one-pixel whole-image translation and must tolerate
bounded edge-coverage changes only when the required region semantics remain
valid. Formal calibration captures, accepted references, and hardware
candidates are exactly 512 by 512 pixels; the 1024 preview is never an
acceptance input, and any other formal extent fails before comparison.

The physical image authority binds all semantic attachments, FinalOutput, and
the window-only capture to one completed submission frame token. Cross-process
calibration is required before a new reference or reference-set member can be
accepted; within-process repetition alone is diagnostic.

Serialized hardware execution collects recoverable package validation failures
and continues after rechecking the authority lock and device. Preflight,
revision/source mutation, lock loss, device loss, and evidence-integrity errors
remain fail-fast. The operational budget is 3,600 seconds per package and 7,800
seconds for the two-package profile.

## Reporting

Every environment-sensitive observation records:

- execution class;
- measurement kind;
- disposition;
- preflight state and evidence digest when required;
- value/unit or stable `not-required`/`not-run` reason;
- threshold only when disposition is `required`;
- replacement lane when required authority is unavailable.

These fields remain outside deterministic Asset/cook/generation identities and
inside existing report/artifact privacy and size bounds.

Windows Vulkan is manually synchronized and required alongside Mac Metal on the
same final revision; no self-hosted runner is required. macOS Vulkan remains
deferred and is not inferred from hosted software/native lanes.
