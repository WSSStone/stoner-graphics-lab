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
| `controlled-physical` | Self-hosted workflow plus successful preflight | All hosted correctness facts plus calibrated RSS and required image/FLIP decisions |
| `local-diagnostic` | Local runner default | Reproduction and observations only; cannot satisfy required hosted/physical closeout authority |

The class is not accepted from an unrestricted application or runner CLI token.
The workflow/profile binding and report provenance identify it. Missing,
conflicting, or caller-promoted authority fails closed.

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
- a 3,900-second complete-package operational timeout;
- an independent 3,600-second native operational timeout inside a 90-minute job.

RSS endpoints, task-VM categories, allocator totals, peak memory, and elapsed
time are `observed`. Hosted execution uses normal production allocator behavior;
validation-only allocator-zone switches cannot manufacture RSS authority.

## Controlled Physical Preflight

Before RSS or image evidence is `required`, the lane proves:

1. exact registered device and backend capability signature;
2. exclusive runner/device/display ownership for the job;
3. exact source revision, workload revision, target profile, and software image;
4. default production allocator behavior;
5. declared warm-up, queue-idle, teardown, sample, and capture protocol;
6. required application-window presentation and readback capability.

Failed preflight produces `Unsupported` with a stable missing prerequisite and
replacement lane. It cannot fall back to hosted or local measurements.

The controlled physical lifecycle retains the 16 MiB post-warm-up-to-terminal
RSS hard limit. Required image acceptance retains exact workload/backend/device-
class baseline selection, region semantics, same-size FLIP, and explicit
maintainer-accepted reference state.

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
