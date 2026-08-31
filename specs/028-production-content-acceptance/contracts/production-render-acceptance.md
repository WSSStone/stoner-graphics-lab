# Production Render Acceptance Contract

## Workload Authority

One production composition is identified by a versioned workload ID and
contains the strict-cooked root, model placement, camera, lights, frame state,
and render settings. The same values feed Vulkan and Metal. Backend-specific
differences are limited to target payload selection, capability-justified
fallback, native presentation, and readback normalization.

Deferred executes the full production acceptance workload. Forward executes a
bounded visible and native-readback smoke with the same root/camera/composition.

## Camera Authority and Calibration Preview

Formal rendering selects one code-owned camera preset by exact workload
revision. The preset contains 16 row-major float32 View values and 16 row-major
float32 Projection values. View must be finite, affine, orthonormal without
scale/shear, and invertible. Projection must be finite, invertible, and match
the engine's positive-X-forward StandardZ perspective convention. Camera
position, ViewProjection, and inverse ViewProjection are derived. Missing,
invalid, or ambiguous presets fail before native submission.

An explicit interactive calibration preview may use the same strict-cooked
root, Renderer realization, Deferred execution, requested backend, and native
application window to propose a candidate. It may accept input and write a
bounded candidate record, but it cannot modify the preset registry, image
baseline state, or any formal report. Formal validation rejects caller camera
overrides and does not read preview input.

Changing a frozen camera advances `WorkloadRevision`; semantic regions,
references, calibration mutations, explicit maintainer acceptance, and required
hardware evidence are repeated for the new revision.

## Native Proof Sequence

1. Prove the requested RHI backend and physical/native execution mode.
2. Realize and publish the complete model snapshot.
3. Submit real backend commands and observe completion synchronization.
4. For visible lanes, present the complete application window or surface.
5. Copy GPU-produced attachments and final output to readback, propagate the
   completed submission frame token through presentation, and verify that the
   resulting authoritative frame bundle has one token.
6. Normalize row pitch, channel order, image origin, and color transfer.
7. Run semantic probes.
8. Select the exact accepted image baseline.
9. Run FLIP and enforce all declared limits.
10. Emit report and window-only capture, then release all ownership.

A deterministic simulation, semantic oracle, software fallback where physical
hardware is required, stale checked-in image, or silently substituted backend
cannot satisfy steps 1-5.

## Mandatory Semantic Probes

- expected dimensions/format and finite values;
- nonblank/non-placeholder pixel distribution;
- expected background and geometry coverage ranges;
- orientation/corner marker or equivalent asymmetric workload evidence;
- current frame token and non-stale submission evidence;
- material-region probes for base color, normal response, metallic/roughness,
  emissive where present, and depth/normal deferred attachments;
- no missing primitive/material region declared by workload inspection.

Every semantic classification uses a versioned bounded region, minimum valid-
sample coverage, and robust statistic; one exact pixel cannot be authoritative.
Every probe must pass before perceptual comparison begins. Required-region
declaration checks are contract checks and do not increment the measured probe
count. Assigning the same lifecycle counter to expected and observed evidence
does not prove a current frame.

## Baseline Selection

The key is exact `(WorkloadRevision, Backend, DeviceClass)`. The runner builds a
canonical signature from registry version, backend implementation, CPU
architecture, adapter family, shader profile, color format, depth format,
sample count, and texture-format family, then derives `DeviceClass` by exactly
one match in `Config/Validation/ProductionContent/DeviceClasses.json`. A CLI or
caller cannot supply an authoritative class token. Marketing device name and
driver version are observations, not key material. Zero or multiple class or
baseline matches is `ImageBaselineMissing` or `ImageBaselineAmbiguous` and
fails the required hardware gate; nearest/fallback selection is forbidden.

An accepted baseline set contains one to three canonically ordered references.
Each reference owns its PNG digest, FLIP policy, and cross-process calibration
digest. Acceptance compares the same candidate to every reference and passes
when every limit passes for at least one reference. All comparisons and the
deterministically selected matching reference are reported.

Every formal Feature 028 calibration capture, accepted reference, and hardware
candidate is exactly 512 by 512 pixels. The 1024-by-1024 interactive preview is
navigation-only and cannot be registered as an acceptance image. A non-512
formal image fails before semantic probes or FLIP.

## Perceptual Policy

- Input is canonical same-size 512-by-512 LDR RGB after color-transfer
  normalization.
- Metric is pinned CPU LDR-FLIP.
- Reference and candidate retain exact frozen-camera pixel registration. The
  comparator cannot translate, scale, crop, warp, resample, or search for a best
  alignment.
- Report mean, p95, max, and bad-pixel fraction where a bad pixel exceeds the
  baseline's FLIP error threshold.
- All four baseline limits must pass.
- Thresholds are fixed reviewed data. Ordinary execution cannot create a
  reference, widen tolerance, choose a nearest device class, or approve output.
- Calibration begins with three independent 20-capture processes and extends
  to at most six only to reproduce multiple modes. A proposed mode needs two
  independent process observations. Blank, real stale-frame, origin, one-pixel
  translation, missing-geometry, material, color-space, and opposite-normal
  mutations must each fail against every accepted reference.

## Lifecycle Profiles

| Profile | Full cycles | Warm-up cycles | Required native scope | RSS rule |
|---|---:|---:|---|---|
| Regular | 20 | 1-2 | Platform-applicable bounded native/headless gates | Hosted/local-diagnostic measurement is observed; physical authority belongs only to the hardware profile |
| Medium Lantern | 1,000 | 1-20 | Hosted Metal endurance authority | 2,000 captures; RSS/task-VM/allocator are observed; exact work/owners/stale/readbacks remain required |
| Medium Sponza | 100 | 1-10 | Hosted Metal heavy-content scale-lifecycle authority | 200 captures; every cycle retains complete closure/realize/render/release work |
| Hardware | 1,000 | 1-20 | Maintainer-local native arm64 macOS Metal and x86_64 Windows Vulkan, in separate runs | Metal required <= 16 MiB after preflight; Windows working-set RSS observed |

Regular uses 900/1,200/600-second package/profile/native operational limits.
The package covers clean/warm/strict/native work, the native child remains
independently capped, and the extra profile-only headroom covers target-toolchain
discovery and orchestration without changing any required work. Medium uses
2,400/2,700/1,800 seconds. Hardware uses 3,600 seconds per package and native
stage inside a 7,800-second serialized profile deadline.

Each cycle performs strict manager bind/request, complete closure, Renderer
realization, deferred render/readback, bounded forward smoke where required,
snapshot release, Asset handle release, manager/backend teardown as declared,
and terminal counter inspection. Counts return to baseline after every cycle or
at the profile's explicitly declared synchronization boundary. Warm-up cycles
count toward the full-cycle total. The RSS origin sample is taken immediately
after the last warm-up cycle and compared with the terminal sample.

## Environment and Measurement Authority

The workflow contract, not an unrestricted CLI token, classifies execution as
`github-hosted`, `maintainer-local-metal`, `maintainer-local-windows-vulkan`, or `local-diagnostic`. Each sensitive
measurement declares one disposition:

- `required`: participates in pass/fail and requires successful authority
  preflight;
- `operational`: fails only when the bounded timeout prevents required work from
  completing;
- `observed`: is serialized for diagnosis/trending and cannot independently
  change the result.

GitHub-hosted medium uses `observed` RSS/task-VM/allocator/elapsed measurements
and operational 2,400/2,700-second package/profile plus 1,800-second native
timeouts inside a 90-minute job. It still requires the exact schema-v4 package
lifecycle (Lantern 1,000/20 and 2,000 captures; Sponza 100/10 and 200 captures),
seven retained readbacks per package, zero terminal owners, stale-handle
rejection, and requested native backend proof.

A maintainer-local physical image authority, and Metal RSS authority, must prove its exact native
OS/architecture/backend and registered device/target, exclusive authority-lock ownership, clean
committed frozen revision, default production allocator, and the declared
sample/presentation protocol. Failed preflight is Unsupported with a
replacement command. Ordinary local or hosted execution cannot claim physical
authority, and aggregation cannot promote `observed` evidence.

## Capture Privacy

- Capture only the application window/render surface, never the desktop.
- Verify capture dimensions, workload/backend in-frame evidence, and digest.
- Reports redact absolute paths, credentials, user names, PID, pointer values,
  and unbounded environment data.
- Canonical report JSON is at most 1 MiB and references at most 64 artifacts;
  each artifact is at most 64 MiB and the aggregate is at most 256 MiB.
- A capture is supporting evidence; GPU readback and semantic probes remain the
  native gate.
