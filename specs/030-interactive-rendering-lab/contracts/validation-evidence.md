# Feature 030 Validation and Evidence

## Meaning of results

All entries below are planned gates. `passed` means a gate actually ran against the named software revision and environment. `unsupported`, `unavailable`, `pending-human-review` and `failed` are distinct states, never aliases for pass. Deterministic fixtures are labeled deterministic. UI smoke, preview export, formal SDR output and live HDR review are separate evidence classes.

Use frozen software SHA checks before/after every formal run; evidence-only commits retain the tested software revision. Working-tree experiments are preliminary and cannot be promoted by relabeling. No Feature 028/029 carry-forward exception applies. The plan itself performs no GPU/human validation.

## Focused suite contract

New proposed `StonerTest --suite` registrations:

| Suite | Required coverage |
| --- | --- |
| `application-free-camera` | Basis/defaults, cadence, reset, aspect adaptation, discontinuity flags |
| `application-ui-input` | Current-frame arbitration, press ownership, focus queue ordering, Escape, clipboard/text, overflow |
| `application-lab-preset` | Schema/digest/atomic import/export, malformed/oversized/path/race/extent cases, unsupported-profile no-mutation and full debug-selection round-trip without capture |
| `renderer-ui-draw` | Packet limits, indices/offsets, scissor, texture generations, callbacks, failure rollback |
| `renderer-ui-color` | Declared display domains, reference white, alpha and exposure invariance |
| `rhi-deferred-submission` | Accepted-versus-completed, real fence status, slot/borrow lifetime, timeout, default Unsupported for missing deferred-submit implementation; optional maintenance1 present/absent/false-feature/forced-off/enablement-failure selection and reacquisition retirement |
| `interactive-lab-lifecycle` | Settings transactions, latest valid request, minimize/resize/close, resource/capture limits |
| `vulkan-ui-native` / `metal-ui-native` | Actual indexed/scissor/mask/texture and direct output execution |

Extend existing RHI pipeline suites for ColorWriteMask and Core platform-file tests for no-replace export. Run regressions `application-window`, `production-camera-preview`, `renderer-output-transform`, `renderer-output-transform-math`, `rhi-presentation-output`, `output-presentation-lifecycle`, `vulkan-output-transform-native`, `metal-output-transform-native`, and existing Asset JSON/material/cooker tests affected by shared yyjson linkage. Names of new suites are deliverables, not current executables.

## Hosted matrix

Create `.github/workflows/feature-030-interactive-lab.yml`, following existing 029 producer/consumer patterns:

- Windows/Linux/macOS Debug and strict Release: six builds; all applicable deterministic suites and public-boundary checks. Keep Apple Objective-C++ private. Windows uses its established CRLF/Unicode path validation conventions.
- Linux ASan+UBSan and independent TSan: input/snapshot/preset/lifecycle suites and affected baseline regressions. No sanitizer claims from ordinary builds.
- Linux Vulkan native with Lavapipe and a virtual display: native UI fixtures and event/lifecycle smoke; reports software rendering, not a physical GPU/display. Exercise auto selection and forced AcquireHistory resize/close in the existing bounded Lantern window lane; Sponza remains a staged medium job with exact package identity.
- macOS Metal native: UI, output/mode lifecycle and HDR nonvisual numeric contracts. No hosted HDR appearance claim. Keep existing dual-architecture compilation/derivation coverage available; native hardware coverage identifies the actual architecture, never inferred universal support.
- Shader producer/consumer: source/SPIR-V validation, deterministic MSL derivation and target-tagged offline metallib cooking; consumer loads strict-cooked UI closure with no source fallback.
- Evidence producer -> independent consumer -> machine aggregate. Required reports and digests are enumerated; missing or stale input fails. Machine aggregate does not close human/physical gates.

A required job cannot be skipped into green. If native window/device access is temporarily absent, record Unsupported/Unavailable, exact manual fallback command and owned follow-up work; the uncovered required closeout gate stays open.

## Physical matrix and interaction

Required physical lanes remain Windows discrete Vulkan SDR using the detected PresentationFence or AcquireHistory path and M4 macOS Metal SDR/PQ1000/PQ2000/EDR1000/EDR2000. Both Lantern and Sponza are covered on every required profile. The fixed selection below concentrates endurance on distinct resource/output paths; it does not turn untested combinations into passes.

| Gate | Required combinations | Minimum execution |
| --- | --- | --- |
| Endurance | Windows Vulkan + Sponza + SDR; M4 Metal + Sponza + SDR; M4 Metal + Lantern + PQ2000; M4 Metal + Lantern + EDR2000 | Four runs, each 120 warmup then 1000 measured presented frames, captures disabled |
| Functional/profile smoke | Both workloads on Windows SDR and on all five M4 profiles: 12 combinations total | Each of the remaining eight combinations runs 120 presented frames, with no excluded warmup; the four endurance runs satisfy their matching smoke entries |
| Lifecycle stress | Sponza on Windows SDR and M4 SDR | 20 cycles each of resize, UI toggle, focus recovery, minimize/restore and font scale replacement; no profile/workload Cartesian product |
| Mode-transition stress | Sponza on Windows; Lantern on M4 | 20 Windows cycles through supported SDR choices with explicit unavailable-profile rejection; 20 M4 cycles SDR -> PQ1000 -> PQ2000 -> EDR1000 -> EDR2000 -> SDR |
| Remaining lifecycle integration | Lantern on each backend; Sponza on M4 for the full mode sequence | At least one cycle of each class not already stress-covered for that workload/backend |
| Formal SDR parity | Both workloads on both physical SDR lanes | Existing frozen-input exact-dimension/probe procedure, unchanged |
| Human navigation / HDR | Both workloads on the required physical lanes; all four M4 HDR profiles | Current hands-on decisions; all four profiles reviewed for both workloads, which may share one bounded human record |

The SDR entry means the workload's existing default SDR profile; additional supported SDR variants are covered by the mode sequence and numeric fixtures. The command file binds expected coverage case ID and gate kind to workload/backend/profile before launch; verify the report against that entry rather than trusting its claimed pass or frame budget. Each smoke/endurance report records the actual case ID, workload/profile, submitted/presented counts and warmup accounting. Smoke checks include visible UI, texture/text validity, effective output identity and zero capture-disabled readbacks; they do not claim 1000-frame endurance. Every run must remain within budgets and drain to the documented ownership state. Resize includes 1:1/16:9 extents within limits; fixed UI exercises font/textures. Capture-disabled counters must show zero image-copy/map/readback waits and zero per-frame queue/device-idle calls in the actual backend.

The four endurance cases are fixed before implementation in `Config/Validation/InteractiveLab/Coverage-v1.json`: Sponza exercises production resource load on both backends, while the two Lantern 2000-nit cases exercise the distinct PQ/EDR output and packing paths. Both peak variants and both scenes still receive native functional coverage and current human HDR review. Add targeted repetition only if a changed resource path, new failure or unresolved platform risk justifies it; record that reason rather than multiplying all dimensions. An unsupported required case remains open and cannot be replaced silently with a smaller workload or historical evidence.

Test absent maintenance1 as successful fallback selection when the underlying device/surface is otherwise usable. Cover both policy branches deterministically, including capability-query failure versus known absence and one failed optional-enable retry. Run the existing Windows Lantern SDR smoke with --lab-vulkan-retirement acquire-history and the existing hosted Lavapipe Lantern lane with auto plus forced-fallback lifecycle/close; keep the four endurance/eight profile-smoke table unchanged. Exercise the preferred native path when advertised/enabled on an available target; if absent everywhere, record that optional branch as unavailable without blocking a successfully executed fallback platform gate or inventing native extension coverage. Test reacquisition sync (not just an image index), immediate resize without replacement reacquisition, third-generation admission rejection/coalescing, 512 MiB/16-record limits, zero-extent resume, unpresented cancellation, oldSwapchain invalidation on creation failure and terminal idle timeout. A normal fallback exit with IdleAssumed may pass with the declared compatibility limitation; unexpected timeout/device loss/forced termination cannot. Failure-injection tests may pass by correctly observing those failed child outcomes, never by relabeling them as clean lab runs. Inject separately delayed render and presentation completions to keep both slots occupied and replace UI textures without invalidating old draws. Verify superseded UI textures retire after all render uses and snapshot leases finish while pending presentation still retains output images/semaphores; also prove an incomplete render prevents that texture retirement. Prove busy slots continue servicing input; inject timeout/device failure and verify bounded cleanup/first-failure reporting. Test display changes, stale mode generations and zero extent using deterministic cases plus applicable physical actions; do not claim a synthetic display-scale event was a physical monitor move.

Maintainer hands-on review covers navigation, RMB/cursor release, typing/drag/scroll isolation, readability, exposure/output changes, preset export/import at changed aspect, UI toggle, focus/minimize recovery and exit. Record observations for the current software/device. Windows Console or RDP sessions follow inherited session/adapter rules: RDP claims application GPU/window output, not physical scanout or Console equivalence; keep cooked/evidence staging on local NTFS during runs.

## UI-off parity and HDR authority

Formal UI-off runs use frozen workload matrices/dimensions and inherited semantic probes, calibration and native readback/presentation identity. Compare each production workload on physical Windows Vulkan/macOS Metal against its applicable Accepted policy reference. The reference can retain its original provenance; the new run must be current. A difference cannot be hidden by alignment, cropping, scaling, resampling or widened tolerances. If formal pixels change, bump the workload revision and obtain a new exact-dimension Candidate and explicit acceptance through the existing workflow.

A bounded Forward fixture proves the shared UI/output integration; production image authority remains the existing Deferred workload path. Compare same output policy and fixture semantics, not arbitrary Forward/Deferred image equality.

HDR scene/UI numeric conformance and format/metadata tests are machine-only. Live macOS review must cover all four profiles at current software, using same-generation native reference white/headroom and the inherited `EDRMetadata=nil` policy. Record UI readability/brightness independently of scene appearance; a composite human record may link all observations. Automation creates only the review request and verifies structure/digests of a human-authored decision. It cannot populate a positive observation, infer pass from screenshots or recycle waived historical feedback.

## Bounded records

New `stoner.interactive-lab-report` v1 JSON identifies run/evidence kind, software SHA, backend/adapter/session, workload/source/cooked identity, exact logical/drawable extents, display/settings generations, effective profiles and white, UI visibility, input cases, lifecycle counts, frame submitted/present-queued/completed counts, readback counters, resource high-water/quiescent counts, limits version and first failure. Add presentationRetirementMode (PresentationFence, AcquireHistory or NativeCallback), optional-extension advertised/enabled status and fallback reason, active/retiring generation and acquisition/presentation counts, estimated swapchain bytes/high-water, shutdownAssurance (Proven, IdleAssumed, Forced or DeviceLost), pre-cleanup outstanding presentation owners, terminal idle count/result/duration and forcedTermination. Render-retired counts and proven presentation-release counts remain separate; IdleAssumed must not increment a proven-release counter. The consumer applies the selected mode's quiescent bounds: retained presentation owners are permitted during fallback drain within caps, completed host cleanup must reach zero, and forced/device-loss runs remain failures. Record the compatibility limitation explicitly even when machine status is passed; Proven cannot be inferred from host counts reaching zero or from idle returning success. GPU time is unavailable until implemented by later profiling; do not fabricate zero.

Each JSON <=1 MiB; <=64 referenced artifacts per report, each <=64 MiB and aggregate <=256 MiB. Paths are safe repository-relative references with SHA-256; no raw logs/buffers in checked-in evidence. SDR PNGs are exact dimensions and have explicit non-authoritative UI-smoke or formal Candidate labels. Raw captures/logs live under ignored `Build/Validation/030`; bounded reports under `Validation/030/{CI,UI,SDR,HDR}`. A human review record includes request identity, software/device/display, observed profile IDs, reviewer decision and the referenced machine report bundle; it is append-only.

## Tooling reuse boundary

Add one small `.github/scripts/interactive_lab_validation.py` entrypoint with `run`, `verify` and `closeout` subcommands, plus one focused `.github/scripts/test_interactive_lab_validation.py`. Keep ordinary separate functions for process orchestration, lab-field checks and a fixed required-case table; do not introduce a validation framework, plugin system or duplicate 028/029 policy engines. Independent consumption means `verify`/`closeout` execute in a fresh consumer process/job from artifact bytes; sharing the same checked-in entrypoint does not allow trusting an in-memory producer result.

| Existing implementation | Reuse in 030 |
| --- | --- |
| `.github/scripts/output_transform_provenance.py` | Call `require_frozen_revision` before/after current authority runs and `artifact` for bounded PNG/JSON identities; add 030 case/settings linkage around these helpers |
| `.github/scripts/verify_output_transform_evidence.py` | Reuse `load_bounded_json` and `validate_artifacts` for bounded parsing, safe paths and digests; run its existing full validators only on actual 029-format records |
| `.github/scripts/compare_output_transform_images.py` and existing formal SDR procedures | Run the inherited exact-image/probe/calibration checks; never copy comparison code or change tolerance/reference policy for UI smoke |
| `.github/scripts/output_transform_common.py` | Reuse applicable decoded-domain numeric comparisons; 030 adds only UI composition vectors, not new HDR scene appearance scoring |
| `.github/scripts/verify_output_transform_architecture.py` | Extend with additive 030 boundary checks; preserve existing checks/default behavior and avoid a new architecture tool |
| Existing 027/029 workflows and commands | Reuse cook/derivation, strict build, sanitizer and native fixture invocation patterns; collect the relevant results once per software revision |

030-specific work is the bounded native argv runner/watchdog, lab fields/counters/case coverage and linkage of current physical and human decisions. Use one `Report-v1.schema.json` with definitions for lab reports, a bundle manifest and distinct human request/decision records, plus `Coverage-v1.json` for the finite required-case table. A schema validates structure; it does not grant authority or infer an approving observation. Machine verification and final human-complete status remain distinct fields/results. Do not submit lab reports to a 029 aggregate that expects 029's different matrix, or relax that aggregate to accept them. The new `closeout` subcommand checks the 030 coverage table and links real inherited SDR results and human-authored decisions without fabricating or relabeling either.

Required reports remain <=1 MiB each with <=64 bounded artifact references. Missing/stale inputs, failed required cases or absent/currently rejected human decisions leave closeout incomplete. Reuse tests retain old behavior; new tests cover only 030-specific rules and representative mutation cases at shared-helper call sites, rather than duplicating all existing validator tests.

## Traceability

FR-001–FR-010 map to input/camera fixtures and hands-on review; FR-011–FR-015 to transactional settings/lifecycle; FR-016–FR-018 to native draw/color/UI-off parity; FR-019–FR-020 to preset/path tests; FR-021–FR-024 to instrumented deferred submission and budgets; FR-025–FR-029 to evidence isolation and closeout. SC-001–SC-008 are all covered; no new full-profiler, editor or rendering-effect acceptance is introduced.
