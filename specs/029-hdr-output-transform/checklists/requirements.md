# Specification Quality Checklist: Renderer HDR Post-Processing & Output Transform

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-09-01
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Implementation Coverage Reconciliation

The table below is the closeout audit from requirement to implementation task,
test oracle, and durable evidence. `Implemented` means the repository contract
and test exist; it does not upgrade a Candidate, Unsupported result, machine HDR
preflight, or missing external run into acceptance.

### Functional Requirements

| Requirement | Owning tasks | Test / inspection oracle | Evidence and current disposition |
|---|---|---|---|
| FR-001 | T024, T028-T031 | `RendererOutputTransformTests.cpp` | `CI/us1-formal-output.json` — implemented/passed |
| FR-002 | T021-T022, T034-T036 | Forward and Deferred regression suites | `CI/us1-formal-output.json` — shared path passed |
| FR-003 | T024-T026, T028 | `renderer-output-transform` invalid-input cases | `CI/us1-formal-output.json` — implemented/passed |
| FR-004 | T020, T028, T031 | fail-closed and no-partial-publication cases | `CI/us1-formal-output.json` — implemented/passed |
| FR-005 | T020, T026, T029-T031 | single-writer/formal-output cases | `CI/us1-formal-output.json` — implemented/passed |
| FR-006 | T029-T030, T043, T051 | stage-order and shader-parameter tests | `CI/us1-formal-output.json`, `CI/us2-color-conformance.json` — passed |
| FR-007 | T057-T063 | `renderer-post-process-insertion` pre-domain cases | `CI/us3-insertion-contract.json` — passed |
| FR-008 | T057-T063 | post-domain and graph-placement cases | `CI/us3-insertion-contract.json` — passed |
| FR-009 | T057, T060-T062 | duplicate/dependency/cycle/order cases | `CI/us3-insertion-contract.json` — passed |
| FR-010 | T057-T063 | invariant, hazard, transfer-claim rejection | `CI/us3-insertion-contract.json` — passed |
| FR-011 | T038, T043, T050 | finite/exactly-once exposure cases | `CI/us2-color-conformance.json` — passed |
| FR-012 | T004-T006, T038, T043 | factor-of-two exposure vectors | `CI/m0-color-authority.json`, `CI/us2-color-conformance.json` — passed |
| FR-013 | T001, T038, T044 | three Strategy identity/default cases | `CI/us2-color-conformance.json` — passed |
| FR-014 | T003-T006, T040, T048 | frozen expected-vector/no-regeneration verifier | `CI/m0-color-authority.json` — passed |
| FR-015 | T004-T005, T028, T043, T051 | non-finite/negative/opaque-alpha cases | `CI/m0-color-authority.json`, `CI/us2-color-conformance.json` — passed |
| FR-016 | T001, T039, T042, T045 | seven-profile schema and runtime matrix | `CI/us2-color-conformance.json` — passed |
| FR-017 | T039, T042, T049-T051 | exactly-once transfer tests | `CI/us2-color-conformance.json` — passed |
| FR-018 | T001, T005, T041, T046, T054 | encoded/decoded and XYZ propagation cases | `CI/us2-color-conformance.json` — passed |
| FR-019 | T004-T006, T040-T041, T048, T054 | 20-repeat CPU/GPU conformance | `CI/m0-color-authority.json`, `CI/us2-color-conformance.json` — passed |
| FR-020 | T015-T017, T023, T030 | typed graph resource/domain validation | `CI/foundation.json`, `CI/us1-formal-output.json` — passed |
| FR-021 | T023, T058, T062-T063 | empty-insertion equivalence | `CI/us1-formal-output.json`, `CI/us3-insertion-contract.json` — passed |
| FR-022 | T059, T064-T065 | named bypass/no-formal-mutation cases | `CI/us3-insertion-contract.json` — passed/non-authoritative |
| FR-023 | T059, T064 | HDR-preserving debug readback/visualization cases | `CI/us3-insertion-contract.json` — passed/non-authoritative |
| FR-024 | T060-T067 | insertion seam and no-temporal-state scan | `CI/us3-insertion-contract.json` — passed; consumed by Feature 030 |
| FR-025 | T068, T071, T078-T083 | resize/reconfigure/stale-resource matrix | `CI/us4-native-lifecycle.json` — passed |
| FR-026 | T007, T012-T014, T068, T071, T083 | zero-drawable pause/restore cases | `CI/foundation.json`, `CI/us4-native-lifecycle.json` — passed |
| FR-027 | T071, T083, T087 | 100-transition ownership/failure matrix | `CI/us4-native-lifecycle.json` — passed |
| FR-028 | T069-T080, T086-T087 | Vulkan/Metal native suites | `CI/us4-native-lifecycle.json` — applicable non-visual paths passed |
| FR-029 | T069-T071, T079-T081, T085 | same-frame token and exact-extent cases | `CI/us4-native-lifecycle.json`; physical M4 probes passed |
| FR-030 | T031, T071, T080, T083-T085 | failure injection/terminal-owner cases | `CI/us4-native-lifecycle.json` — passed |
| FR-031 | T107-T112 | workflow matrix and strict build/sanitizer jobs | Workflow defined; final same-revision hosted run pending T112 |
| FR-032 | T070, T076-T079, T084-T087, T104 | Metal PQ/EDR native suite and four live probes | Four physical M4 non-visual probes passed; committed request pending |
| FR-033 | T093-T103 | v3 workload/baseline/Candidate state machine | M4 v3 Candidates present; Windows Candidates and acceptance pending |
| FR-034 | T088, T094, T100 | immutable-v2 byte/digest preservation | `SDR/feature-028-v2-preservation.json` — passed |
| FR-035 | T088-T090, T094-T096 | Candidate-only and missing-Accepted tests | M4 Candidate manifests prove fail-closed state |
| FR-036 | T090, T095 | exact-dimension/mismatch-before-FLIP cases | Python runner tests and M4 exact PNGs — passed |
| FR-037 | T090, T095 | translation/normalization/one-pixel mutation cases | calibration and runner tests — passed |
| FR-038 | T088, T093-T103 | fresh-authority key/revision/calibration rules | Preliminary M4 captures retained; same-SHA M4/Windows and maintainer authority pending |
| FR-039 | T092, T099-T103 | carry-forward rejection aggregation tests | Rejection test passed; fresh Windows authority pending |
| FR-040 | T093, T095, T100-T102 | exact 512×512, sampleCount=1 and bounded run | v2 preservation + M4 Candidate records — passed locally |
| FR-041 | T089, T095-T096, T109 | lossless PNG/canonical JSON/bounds verifier | M4 bundle within bounds; closeout bundle pending |
| FR-042 | T026, T047, T065, T075, T078-T085 | normalized diagnostic/probe identity tests | `CI/us1-*`, `CI/us4-*`; physical M4 probe identity complete |
| FR-043 | T021-T023, T036-T037, T057-T067 | Forward/Deferred and insertion matrices | `CI/us1-formal-output.json`, `CI/us3-insertion-contract.json` — passed |
| FR-044 | T067, T110 | architecture/scope scan | `verify_output_transform_architecture.py` — zero scope findings |
| FR-045 | T091-T092, T097-T099, T104-T106 | request/attestation/aggregation contract tests | Machine preflight passed; live human observations pending |
| FR-046 | T084, T092, T099, T108 | Windows-HDR-no-claim tests/workflow record | Contract passed; no Windows HDR lane or claim exists |

### Success Criteria

| Criterion | Owning tasks | Test / inspection oracle | Evidence and current disposition |
|---|---|---|---|
| SC-001 | T020-T037 | Forward/Deferred formal-output suites | `CI/us1-formal-output.json` — passed |
| SC-002 | T001-T006, T038-T056 | ≥32 HDR vector CPU/GPU decoded-domain checks | `CI/m0-color-authority.json`, `CI/us2-color-conformance.json` — passed |
| SC-003 | T001-T006, T039-T056 | ≥16 vectors for each of seven profiles | `CI/us2-color-conformance.json` — passed |
| SC-004 | T004-T006, T040-T041, T048, T054 | 20-repeat normalized determinism | M0/US2 evidence — passed |
| SC-005 | T057-T067 | empty/pre/post/both insertion matrix | `CI/us3-insertion-contract.json` — passed |
| SC-006 | T059, T064-T065 | bypass stage identity/non-authority cases | `CI/us3-insertion-contract.json` — passed |
| SC-007 | T068-T071, T083, T087 | three extents + zero + restore + mode/failure matrix | `CI/us4-native-lifecycle.json` — passed |
| SC-008 | T069-T087 | successful native same-frame/extent/owner checks | `CI/us4-native-lifecycle.json`; M4 probes passed |
| SC-009 | T107-T112 | three-platform strict/sanitizer/Lavapipe workflow | Definition/tests passed; final hosted same-revision run pending |
| SC-010 | T088, T093-T100 | v2 byte preservation and separate v3 authority | `SDR/feature-028-v2-preservation.json` — passed |
| SC-011 | T090, T095 | mismatch, translation, normalization, one-pixel rejection | Python tests and calibration — passed |
| SC-012 | T101-T103 | explicit acceptance for every new SDR reference | Preliminary M4 Candidates retained; formal same-SHA M4/Windows and maintainer acceptance pending |
| SC-013 | T089, T096, T109 | evidence count/size/type/digest/privacy bounds | Verifier tests passed; final closeout bundle pending |
| SC-014 | T111 | Features 013/015/018/019/027/028 + 029 regressions | `CI/regressions.json` — local pass; refresh after final edits |
| SC-015 | T091-T099, T104-T106 | four Metal HDR preflights + four live observations | Four non-visual probes passed; exact-revision request/human pass pending |
| SC-016 | T023, T030-T031, T037 | pass/resource/full-image/readback-count checks | `CI/us1-formal-output.json` — passed |

### External Closeout Gates

- [ ] Reproduce physical M4 Metal Lantern and Sponza v3 exact SDR Candidates
  at the frozen software revision with calibration/native sidecars; retain the
  preliminary working-tree Candidates without promotion or relabeling.
- [x] Physical M4 Metal PQ1000/PQ2000/EDR1000/EDR2000 machine preflights
  complete with `EDRMetadata=nil`; no visual decision inferred.
- [ ] Reproduce the HDR preflight from the exact committed revision and create
  `Validation/029/HDR/hdr-live-review-request.json`.
- [ ] Generate fresh physical Windows Vulkan Lantern and Sponza v3 Candidates.
- [ ] Maintainer explicitly accepts or rejects all four SDR tuples.
- [ ] Maintainer personally views all four HDR modes and authors immutable
  linked attestations.
- [ ] Run final same-revision hosted matrix and closeout aggregation.

## Notes

- Validation iteration 1 passed all checklist items. Renderer, Render Graph,
  RHI, Vulkan, Metal, and the existing acceptance workload names appear only as
  roadmap-owned product scope and architecture boundaries; the specification
  does not prescribe implementation classes, source files, or libraries.
- Clarification iteration 2 froze three SDR tone-map versions with Khronos PBR
  Neutral v1 as default; a separate ACES-style HDR viewing transform; the full
  SDR plus 1000/2000-nit PQ and scRGB/EDR output-device matrix; and RGBA16F
  linear Rec.709/sRGB-D65 as the canonical working space.
- Feature 028 v2 references remain immutable historical evidence. Feature 029
  SDR changes require successor workload revisions, fresh exact-size Candidates,
  explicit maintainer acceptance, both existing SDR physical authorities, no
  reuse of the Feature 028 Windows carry-forward, and bounded PNG/JSON evidence.
  HDR visual acceptance is macOS Metal-only, covers PQ and EDR/scRGB by live
  maintainer inspection, and cannot be inferred by automated comparison;
  Windows claims no HDR validation.
- The clarified HDR scope intentionally supersedes Roadmap 2.3's earlier
  HDR10/EDR exclusion. Align the roadmap and its mirrored roadmap specification,
  then proceed to `/speckit-plan`; another clarification pass is not required.
