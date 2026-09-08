# Feature 030 Interactive Lab Fixture Inventory

This inventory is the bounded test and validation plan for T004. The fixture
IDs and suite names below are planned coverage names; they are not executable
suites until their owning implementation tasks provide the assertions. No
placeholder suite may be registered with an unconditional pass.

Every fixture follows failure-first order: first establish the invalid,
unsupported, stale, over-budget, or authority-confusing behavior named in its
row; then add the success assertion. A native or human gate is evidence work,
not a deterministic fixture pass. `passed`, `unsupported`, `unavailable`,
`pending-human-review`, and `failed` remain distinct results.

## Configuration authority

The fixture checks consume the two T002 records directly. They do not duplicate
or relax their limits.

| Record | Required checks |
| --- | --- |
| `Config/Validation/InteractiveLab/Limits-v1.json` | JSON parsing; two frame slots; one window/workload; drawable 4096 x 4096 and 7,864,320-pixel cap; 1 GiB slot-target budget; camera speed/FOV/near/far/time bounds; 1e-4 and 5e-4 camera tolerances; 4096 events; UI packet/texture/upload/generation budgets; capture, preset, diagnostic, retry, transition, drain, and watchdog deadlines. |
| `Config/Validation/InteractiveLab/Coverage-v1.json` | Four endurance cases at 120 warmup + 1,000 measured presented frames; eight remaining smoke cases at 120 presented frames; seven lifecycle/integration rows; 12 required workload/profile combinations; four formal SDR gates; four current navigation gates; eight current M4 HDR human gates. |
| `Config/Validation/OutputTransform/Profiles.json` | The seven existing profile IDs and exact tolerance policies referenced by the limits record. |
| `Config/Validation/OutputTransform/Workloads/Lantern-v3.json` / `Sponza-v3.json` | The exact revisions `production-content-lantern-v3` and `production-content-sponza-v3`; no guessed workload identity. |

The required functional profile matrix is Windows discrete Vulkan plus
`Sdr.sRGB.v1` for Lantern and Sponza, and M4 Metal plus
`Sdr.sRGB.v1`, both PQ profiles, and both EDR profiles for each workload.
The other two existing SDR variants are exercised by the Windows mode
transition sequence and numeric settings fixtures. Interactive smoke and
endurance are non-authoritative; formal SDR and current human HDR decisions
remain separate evidence classes.

## Fixture ownership and failure-first expectations

| Planned suite | Owner | Planned fixture IDs | Failure-first expectation |
| --- | --- | --- | --- |
| `application-free-camera` | Application camera/controller | `camera-basis-defaults`, `camera-cadence-30-60-120`, `camera-reset-discontinuity`, `camera-current-aspect-restore`, `camera-finite-bounds`, `camera-mixed-motion-timestamps` | Reject non-finite or negative elapsed time, clamp an oversized step, reject invalid speed/FOV/near/far, and expose missing discontinuity/aspect flags before asserting valid navigation and cadence equivalence. |
| `application-ui-input` | Application input/ownership | `input-first-widget-click`, `input-drag-outside-release`, `input-wheel-owner`, `input-simultaneous-navigation`, `input-transfer-release-rearm`, `input-focus-queue-order`, `input-pointer-warp`, `input-escape-cancel`, `input-text-invalid-nonbmp`, `input-clipboard-unavailable`, `input-scale-100-150-200`, `input-event-overflow` | Prove a UI-owned key, drag, wheel, text edit, clipboard failure, focus transfer, synthetic warp, or overflow cannot move the camera or leave capture stuck; only a release and fresh press re-arms navigation. |
| `application-lab-preset` | Application preset transaction/Core boundary | `preset-roundtrip-float32`, `preset-duplicate-unknown-key`, `preset-oversized-deep-invalid-unicode`, `preset-digest-mutation`, `preset-wrong-workload`, `preset-unsupported-profile-no-mutation`, `preset-changed-aspect`, `preset-zero-extent-pending`, `preset-debug-selection-no-capture`, `preset-protected-path`, `preset-no-replace-race` | Establish that malformed, oversized, stale, digest-mutated, unsupported, or protected imports/exports fail atomically, preserve the previous state, do not touch Accepted data, and do not enqueue a capture; only a valid same-workload import commits at the current drawable aspect. |
| `renderer-ui-draw`, `ui-texture-registry` | Renderer UI packet/texture registry | `ui-packet-vertex-index-command-limits`, `ui-packet-triangle-and-offsets`, `ui-packet-finite-bounds`, `ui-scissor-negative-scale`, `ui-texture-stale-generation`, `ui-texture-upload-order`, `ui-reset-state-callback`, `ui-allocation-rollback`, `ui-budget-retirement` | Reject non-finite or out-of-range vertices, indices, commands, offsets, callbacks, texture generations, and budget products before allocation; prove old generations stay leased until all render uses finish. |
| `renderer-ui-color` | Renderer composition/color | `ui-domain-srgb-linear-alpha`, `ui-color-primary-and-coverage`, `ui-reference-white`, `ui-exposure-invariance`, `ui-rgb-mask-alpha-one`, `ui-format-metadata`, `ui-hdr-numeric-tolerance`, `ui-no-double-transfer` | Establish a failure for wrong domain decode, stale EDR white, alpha-mask changes, double gamut/transfer, or widened formal tolerance before validating display-linear blend results at -3/0/+3 stops. |
| `rhi-deferred-submission` | RHI and backend seam | `deferred-default-unsupported`, `deferred-accepted-vs-completed`, `deferred-unsignaled-render-fence`, `deferred-presentation-lease`, `deferred-two-slot-bound`, `deferred-timeout-device-loss`, `deferred-maintenance1-unsupported` | The default path must return `Unsupported` without submitting; delayed render and presentation completions must retain their independent owners; timeout/device loss and absent maintenance1 must remain explicit failures rather than synchronous success. |
| `interactive-lab-lifecycle` | Demo lab session/runtime | `lifecycle-coherent-settings-revision`, `lifecycle-latest-valid-pending-request`, `lifecycle-zero-extent`, `lifecycle-resize-scale-focus-minimize`, `lifecycle-ui-texture-retry`, `lifecycle-capture-queue-staging`, `lifecycle-diagnostic-ring`, `lifecycle-drain-quiescent`, `lifecycle-pending-preset` | Establish stale-generation, invalid-request, zero-extent, retry, queue, ownership, and deadline failures first; then prove coherent next-frame settings, event service while busy, bounded recovery, and declared quiescent ownership after drain. |
| `vulkan-ui-native` / `metal-ui-native` | Backend native fixtures | `native-indexed-scissor-texture`, `native-rgb-mask-alpha`, `native-direct-acquired-output`, `native-no-readback-capture-disabled`, `native-delayed-retirement` | A missing capability, invalid native packet, indirect readback/reupload path, or premature release must report unsupported/failed; native execution evidence cannot be inferred from deterministic fixtures or hosted build-only success. |

The native suite names are deliverables from the validation contract, not
current test registrations. The bundled private ImGui/font and shader closure
are covered by T003 architecture checks and later producer/consumer work; this
inventory does not add a second checker or a source fallback.

## Bounded validation-script inventory

The single planned entrypoint is `.github/scripts/interactive_lab_validation.py`.
It composes existing provenance and artifact helpers and has no framework,
plugin layer, or duplicate 028/029 policy engine.

| Command | Bounded checks and failure-first cases |
| --- | --- |
| `run` | Parse an argv-only command file with no shell expansion; require a unique Coverage-v1 case ID, gate kind, workload revision, backend, and profile; enforce the positive frame budget; guard the software revision before and after; apply the 10-second hung-child watchdog and retain residual-owner failure context. |
| `verify` | Parse a fresh report process; reject stale/wrong case identity, false smoke-as-endurance claims, missing submitted/presented/completed counters, limit overflow, nonzero capture-disabled readbacks or per-frame queue/device-idle calls, unsafe paths, reports over 1 MiB, more than 64 artifacts, artifacts over 64 MiB, or aggregate artifacts over 256 MiB. |
| `closeout` | Close only the finite Coverage-v1 table; require the four endurance, eight smoke, lifecycle, four formal SDR, and current human links; keep machine and human status separate; reject missing/rejected current human decisions, historical carry-forward, or unavailable required cases presented as pass. |
| `test_interactive_lab_validation.py` | Mutation cases for argv/frozen revision, stale case/digest/path/size, unsupported result, false smoke endurance, missing human decision, and shared-helper integration. These tests must fail against the unimplemented behavior before passing assertions are accepted. |

## Coverage case closure

Coverage-v1 currently defines these execution rows. Case IDs are unique across
the four endurance, eight smoke, and seven lifecycle rows.

| Kind | Case IDs and required execution |
| --- | --- |
| Endurance (4) | `windows-vulkan-sponza-sdr-endurance`, `macos-metal-sponza-sdr-endurance`, `macos-metal-lantern-pq2000-endurance`, `macos-metal-lantern-edr2000-endurance`; each has captures disabled, 120 warmup presented frames, and 1,000 measured presented frames. |
| Remaining smoke (8) | `windows-vulkan-lantern-sdr-smoke`, `macos-metal-lantern-sdr-smoke`, `macos-metal-lantern-pq1000-smoke`, `macos-metal-lantern-edr1000-smoke`, `macos-metal-sponza-pq1000-smoke`, `macos-metal-sponza-pq2000-smoke`, `macos-metal-sponza-edr1000-smoke`, `macos-metal-sponza-edr2000-smoke`; each has 120 presented frames with no excluded warmup. Each smoke/endurance case requires visible UI, texture/text validity, effective output identity, and zero capture-disabled readbacks. |
| Lifecycle stress (2) | Windows Vulkan Sponza SDR and M4 Metal Sponza SDR; 20 cycles each of resize, UI toggle, focus recovery, minimize/restore, and font-scale replacement. |
| Mode-transition stress (2) | Windows Vulkan Sponza through all supported SDR choices with explicit unavailable-profile rejection; M4 Metal Lantern through `Sdr.sRGB.v1 -> Hdr.PQ.Rec2020.1000.v1 -> Hdr.PQ.Rec2020.2000.v1 -> Hdr.Linear.1000.v1 -> Hdr.Linear.2000.v1 -> Sdr.sRGB.v1`; 20 cycles each. |
| Remaining integration (3) | Lantern on each backend receives missing lifecycle classes at least once; M4 Sponza receives the full mode sequence at least once, with already stress-covered classes recorded as such. |

Functional matrix closure is 12 unique lane/workload/profile combinations:
Windows Vulkan Lantern/Sponza SDR plus M4 Metal Lantern/Sponza across SDR,
PQ1000, PQ2000, EDR1000, and EDR2000. The four endurance rows satisfy their
matching functional smoke combinations; the eight smoke rows cover the other
eight combinations.

Formal SDR gates are the four Windows/M4-by-Lantern/Sponza combinations at
`Sdr.sRGB.v1`, UI disabled, current frozen software, exact workload dimensions,
semantic probes, and inherited calibration policy. Current human navigation
gates cover both workloads on both physical lanes. Current human HDR gates
cover both workloads on M4 Metal for all four HDR profiles. Automation may
prepare and verify links but cannot author an HDR observation or promote UI
smoke to formal authority.

## Requirement traceability

The task references below reproduce the requirement-to-task coverage in
`specs/030-interactive-rendering-lab/tasks.md`; the fixture column identifies
where the planned assertion belongs.

| Requirement | Planned fixture/script coverage | Tasks |
| --- | --- | --- |
| FR-001 | `interactive-lab-lifecycle/lifecycle-coherent-settings-revision`, `run` | T012, T030, T031, T032 |
| FR-002 | `application-free-camera/camera-basis-defaults` | T014, T016, T024, T025, T034 |
| FR-003 | `application-free-camera/camera-finite-bounds`, `interactive-lab-lifecycle/lifecycle-coherent-settings-revision` | T014, T016, T024, T025, T034 |
| FR-004 | `application-free-camera/camera-cadence-30-60-120`, `camera-mixed-motion-timestamps` | T014, T016, T024, T025, T034 |
| FR-005 | `application-ui-input/input-pointer-warp`, `input-escape-cancel`, `input-focus-queue-order` | T014, T016, T024, T025, T034 |
| FR-006 | `application-free-camera/camera-reset-discontinuity`, `camera-current-aspect-restore` | T014, T016, T024, T025, T034 |
| FR-007 | `application-ui-input/input-text-invalid-nonbmp`, `input-clipboard-unavailable` | T035, T039, T040, T042, T043, T054, T057, T074 |
| FR-008 | `application-ui-input/input-first-widget-click`, `input-drag-outside-release`, `input-transfer-release-rearm` | T035, T039, T040, T042, T043, T054, T057, T074 |
| FR-009 | `application-ui-input/input-escape-cancel`, `interactive-lab-lifecycle/lifecycle-resize-scale-focus-minimize` | T035, T039, T040, T042, T043, T054, T057, T074 |
| FR-010 | `application-ui-input/input-scale-100-150-200`, `renderer-ui-draw/ui-scissor-negative-scale` | T035, T039, T040, T042, T043, T054, T057, T074 |
| FR-011 | `interactive-lab-lifecycle/lifecycle-coherent-settings-revision`, `lifecycle-diagnostic-ring` | T058, T060, T061, T062, T063, T064, T065, T068 |
| FR-012 | `interactive-lab-lifecycle/lifecycle-latest-valid-pending-request`, Coverage-v1 matrix | T058, T060, T061, T062, T063, T064, T065, T068 |
| FR-013 | `interactive-lab-lifecycle/lifecycle-coherent-settings-revision`, `lifecycle-latest-valid-pending-request` | T058, T060, T061, T062, T063, T064, T065, T068 |
| FR-014 | `interactive-lab-lifecycle/lifecycle-latest-valid-pending-request`, `application-lab-preset/preset-unsupported-profile-no-mutation` | T058, T060, T061, T062, T063, T064, T065, T068 |
| FR-015 | `interactive-lab-lifecycle/lifecycle-coherent-settings-revision`, `renderer-ui-color/ui-no-double-transfer` | T058, T060, T061, T062, T063, T064, T065, T068 |
| FR-016 | `renderer-ui-draw/ui-packet-vertex-index-command-limits`, `ui-texture-stale-generation`, native indexed/scissor fixtures | T036, T044, T045, T050, T051, T052, T055, T056 |
| FR-017 | `renderer-ui-color/ui-reference-white`, `ui-exposure-invariance`, `ui-rgb-mask-alpha-one` | T048, T053, T069, T071, T072, T073, T075 |
| FR-018 | `run`, `verify`, formal SDR gate linkage | T070, T076, T077, T119, T120 |
| FR-019 | `application-lab-preset/preset-roundtrip-float32`, `preset-debug-selection-no-capture`, `preset-no-replace-race` | T078, T079, T080, T081, T082, T083, T084, T085, T086, T087, T088, T089, T090, T091, T092 |
| FR-020 | `application-lab-preset/preset-wrong-workload`, `preset-changed-aspect`, `preset-zero-extent-pending`, `preset-unsupported-profile-no-mutation` | T078, T079, T080, T081, T082, T083, T084, T085, T086, T087, T088, T089, T090, T091, T092 |
| FR-021 | `rhi-deferred-submission/deferred-accepted-vs-completed`, `deferred-unsignaled-render-fence`, `verify` | T007, T017, T019, T020, T021, T022, T023, T026, T027, T028, T033, T066, T098, T099 |
| FR-022 | `interactive-lab-lifecycle/lifecycle-ui-texture-retry`, `lifecycle-capture-queue-staging`, `renderer-ui-draw/ui-budget-retirement` | T002, T015, T093, T095, T096, T097, T094, T101, T102, T103, T104 |
| FR-023 | `interactive-lab-lifecycle/lifecycle-resize-scale-focus-minimize`, Coverage-v1 lifecycle rows | T002, T015, T093, T095, T096, T097, T094, T101, T102, T103, T104 |
| FR-024 | `interactive-lab-lifecycle/lifecycle-diagnostic-ring`, `verify` | T002, T015, T093, T095, T096, T097, T094, T101, T102, T103, T104 |
| FR-025 | Formal SDR gate linkage, `run` frozen guard, `verify` authority separation | T012, T031, T091, T100, T105, T108, T113, T119, T120 |
| FR-026 | `verify` bounded report/artifact checks, Coverage-v1 authority separation | T012, T031, T091, T100, T105, T108, T113, T119, T120 |
| FR-027 | `verify` stale software/Candidate mutation, formal SDR linkage | T012, T031, T091, T100, T105, T108, T113, T119, T120 |
| FR-028 | M4 current human HDR gate linkage, `closeout` human/machine separation | T004, T105, T106, T109, T110, T114, T115, T116, T117, T118, T121, T122, T123 |
| FR-029 | All focused suites, native evidence rows, `closeout`, current human gates | T004, T105, T106, T109, T110, T114, T115, T116, T117, T118, T121, T122, T123 |

## Success-criteria traceability

| Criterion | Planned fixture/script coverage | Tasks |
| --- | --- | --- |
| SC-001 | Full Coverage-v1 functional matrix, current navigation gates, `application-free-camera`, lifecycle settings/output fixtures | T034, T068, T121 |
| SC-002 | `application-free-camera` cadence plus all `application-ui-input` ownership/text/focus fixtures | T014, T035, T034, T057 |
| SC-003 | `interactive-lab-lifecycle/lifecycle-coherent-settings-revision`, `lifecycle-latest-valid-pending-request`, `verify` | T058, T068, T093, T104 |
| SC-004 | `application-ui-input/input-scale-100-150-200`, `renderer-ui-color`, all eight M4 current HDR human gates | T069, T074, T077, T121, T122 |
| SC-005 | Four endurance, eight smoke, seven lifecycle rows; `rhi-deferred-submission`; budget and quiescent-state fixtures | T015, T033, T104, T115, T116, T117, T118 |
| SC-006 | All `application-lab-preset` fixtures, including digest, malformed, bounds, changed-aspect, pending, protected-path, and race cases | T078, T079, T083, T089, T092 |
| SC-007 | Four formal SDR gates, frozen guard, exact-dimension/probe linkage, `verify` authority checks | T070, T119, T120 |
| SC-008 | `run`/`verify`/`closeout`, hosted/native evidence identity, four current navigation gates, eight current HDR human gates, explicit unavailable handling | T114, T121, T122, T123 |

This document records planned assertions and coverage closure only. It does not
claim that any fixture, native lane, formal SDR result, or human decision has
passed.
