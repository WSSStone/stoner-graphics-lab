# Tasks: Core Foundation Math Library

**Input**: Design documents from `/specs/004-core-math-library/`  
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/core-math-api.md, quickstart.md

**Tests**: Required by the feature specification and roadmap. Test tasks are listed before their corresponding implementation tasks.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- Each task includes exact file paths

## Path Conventions

- **Core public headers**: `Source/Core/Public/Core/`
- **Core implementation**: `Source/Core/Private/`
- **Tests**: `Tests/`
- **Feature docs**: `specs/004-core-math-library/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare the Core math header locations and test harness used by every story.

- [X] T001 Create `Tests/CoreMathTests.h` with a `RunCoreMathTests()` declaration and shared test result type compatible with the existing test harness
- [X] T002 Create `Tests/CoreMathTests.cpp` with a minimal Core math test harness scaffold that compiles but contains no passing feature assertions yet
- [X] T003 Update `Tests/Main.cpp` to call `RunCoreMathTests()` after Core foundation tests and return nonzero when Core math verification fails
- [X] T004 Verify `Tests/SConscript` discovers `Tests/CoreMathTests.cpp` without explicit file list changes
- [X] T005 [P] Create empty public header shells with `#pragma once` and `namespace Stoner::Core` in `Source/Core/Public/Core/FMath.h`, `Source/Core/Public/Core/FVector2.h`, `Source/Core/Public/Core/FVector3.h`, `Source/Core/Public/Core/FVector4.h`, `Source/Core/Public/Core/FMatrix4x4.h`, and `Source/Core/Public/Core/FQuat.h`
- [X] T006 [P] Create empty public header shells with `#pragma once` and `namespace Stoner::Core` in `Source/Core/Public/Core/FTransform.h`, `Source/Core/Public/Core/FColor.h`, `Source/Core/Public/Core/FBox.h`, `Source/Core/Public/Core/FSphere.h`, and `Source/Core/Public/Core/FPlane.h`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish shared math conventions and minimal scaffolding before user story implementation.

**CRITICAL**: No user story work should begin until this phase is complete.

- [X] T007 Add Core math coordinate, row-major matrix, tolerance, and invalid-input convention comments to `Source/Core/Public/Core/FMath.h`
- [X] T008 Add failing build-only include verification for all focused math headers in `Tests/CoreMathTests.cpp`
- [X] T009 Update `Source/Core/Public/Core/CoreMinimal.h` to include all focused math headers from `specs/004-core-math-library/contracts/core-math-api.md`
- [X] T010 Run `scons` from the repository root and fix scaffold build errors in `Source/Core/Public/Core/`, `Tests/CoreMathTests.cpp`, or `Tests/Main.cpp`

**Checkpoint**: Shared math scaffolding builds, and user story test-first work can begin.

---

## Phase 3: User Story 1 - Use Core Spatial Values in Engine Code (Priority: P1) MVP

**Goal**: Developers can use Core-provided vectors, matrices, quaternions, transforms, and scalar math utilities without higher-layer dependencies.

**Independent Test**: Build and run Core-only verification that creates and combines vectors, matrices, quaternions, transforms, and scalar math results without including RHI, Backend, Renderer, or Application headers.

### Tests for User Story 1

- [X] T011 [US1] Add failing `FMath` constants, clamp, min, max, abs, lerp, degree/radian conversion, trigonometric, square root, and near-equality verification cases in `Tests/CoreMathTests.cpp`
- [X] T012 [US1] Add failing `FVector2`, `FVector3`, and `FVector4` construction, component access, arithmetic, scalar operation, dot product, length, and near-equality verification cases in `Tests/CoreMathTests.cpp`
- [X] T013 [US1] Add failing `FVector3` cross product, zero vector normalization, safe normalization, and NaN/infinity input behavior verification cases in `Tests/CoreMathTests.cpp`
- [X] T014 [US1] Add failing `FMatrix4x4` identity, component construction, row-major layout, multiplication, transpose, inverse success/failure, and point/vector transform verification cases in `Tests/CoreMathTests.cpp`
- [X] T015 [US1] Add failing `FQuat` identity, component construction, normalization, safe normalization, multiplication/composition, and matrix-compatible rotation verification cases in `Tests/CoreMathTests.cpp`
- [X] T016 [US1] Add failing `FTransform` identity, translation/rotation/scale construction, point transform, direction transform, composition, and inverse success/failure verification cases in `Tests/CoreMathTests.cpp`

### Implementation for User Story 1

- [X] T017 [P] [US1] Implement `FMath` constants and scalar helpers in `Source/Core/Public/Core/FMath.h`
- [X] T018 [P] [US1] Implement `FVector2` value type and operations in `Source/Core/Public/Core/FVector2.h`
- [X] T019 [P] [US1] Implement `FVector3` value type, dot product, cross product, and safe normalization in `Source/Core/Public/Core/FVector3.h`
- [X] T020 [P] [US1] Implement `FVector4` value type and operations in `Source/Core/Public/Core/FVector4.h`
- [X] T021 [US1] Implement `FMatrix4x4` row-major value type, multiplication, transpose, inverse handling, and point/vector transforms in `Source/Core/Public/Core/FMatrix4x4.h`
- [X] T022 [US1] Implement `FQuat` identity, construction, normalization, composition, and matrix-compatible rotation behavior in `Source/Core/Public/Core/FQuat.h`
- [X] T023 [US1] Implement `FTransform` translation, rotation, scale, composition, point transform, direction transform, and inverse handling in `Source/Core/Public/Core/FTransform.h`
- [X] T024 [US1] Review `Source/Core/Public/Core/CoreMinimal.h` to ensure it exposes `FMath`, `FVector2`, `FVector3`, `FVector4`, `FMatrix4x4`, `FQuat`, and `FTransform`
- [X] T025 [US1] Run `scons` and `Build/Mac/Debug/Tests/StonerTest` locally, then fix any US1 failures in `Source/Core/Public/Core/` or `Tests/CoreMathTests.cpp`

**Checkpoint**: User Story 1 should pass independently and provide the MVP Core spatial math vocabulary.

---

## Phase 4: User Story 2 - Validate Geometry and Color Building Blocks (Priority: P2)

**Goal**: Developers can represent colors, boxes, spheres, and planes consistently in Core without Renderer-owned dependencies.

**Independent Test**: Build and run Core-only verification that constructs colors and basic geometric primitives, then verifies conversion, containment, combination, classification, and invalid-state behavior.

### Tests for User Story 2

- [X] T026 [US2] Add failing `FColor` float RGBA construction, byte-channel construction, transparent/opaque defaults, clamping, rounding, conversion, and comparison verification cases in `Tests/CoreMathTests.cpp`
- [X] T027 [US2] Add failing `FBox` empty/default state, min/max construction, point addition, box combination, validity, containment, center, and extent verification cases in `Tests/CoreMathTests.cpp`
- [X] T028 [US2] Add failing `FSphere` center/radius construction, validity, negative radius handling, point containment, and boundary tolerance verification cases in `Tests/CoreMathTests.cpp`
- [X] T029 [US2] Add failing `FPlane` normal/distance construction, point construction, degenerate construction, signed distance, and front/back/on-plane classification verification cases in `Tests/CoreMathTests.cpp`

### Implementation for User Story 2

- [X] T030 [P] [US2] Implement `FColor` float and byte-channel behavior in `Source/Core/Public/Core/FColor.h`
- [X] T031 [P] [US2] Implement `FBox` empty/default state, construction, point addition, box combination, containment, center, and extent behavior in `Source/Core/Public/Core/FBox.h`
- [X] T032 [P] [US2] Implement `FSphere` construction, validity, radius handling, and containment behavior in `Source/Core/Public/Core/FSphere.h`
- [X] T033 [P] [US2] Implement `FPlane` construction, validity, signed distance, and point classification behavior in `Source/Core/Public/Core/FPlane.h`
- [X] T034 [US2] Review `Source/Core/Public/Core/CoreMinimal.h` to ensure it exposes `FColor`, `FBox`, `FSphere`, and `FPlane`
- [X] T035 [US2] Run `scons` and `Build/Mac/Debug/Tests/StonerTest` locally, then fix any US2 failures in `Source/Core/Public/Core/` or `Tests/CoreMathTests.cpp`

**Checkpoint**: User Stories 1 and 2 should both pass without higher-layer dependencies.

---

## Phase 5: User Story 3 - Confirm Cross-Platform Math Consistency (Priority: P3)

**Goal**: Maintainers can verify that Core math behavior is deterministic across supported platforms within documented floating-point tolerances.

**Independent Test**: Run the Core math verification suite on each supported platform and compare vector, matrix, quaternion, transform, color, primitive, tolerance, and aggregate include behavior.

### Tests for User Story 3

- [X] T036 [US3] Add aggregate include verification for `Core/CoreMinimal.h` exposing every math header from `specs/004-core-math-library/contracts/core-math-api.md` in `Tests/CoreMathTests.cpp`
- [X] T037 [US3] Add higher-layer isolation verification that Core math tests include no RHI, Backend, Renderer, Application, platform windowing, graphics API, or physics headers in `Tests/CoreMathTests.cpp`
- [X] T038 [US3] Add cross-platform diagnostic output for coordinate convention, matrix layout, tolerance constants, pointer-sized expectations, and representative math results in `Tests/CoreMathTests.cpp`
- [X] T039 [US3] Add baseline-equivalence verification cases that future optimized math paths can reuse for vectors, matrices, quaternions, transforms, colors, boxes, spheres, and planes in `Tests/CoreMathTests.cpp`

### Implementation for User Story 3

- [X] T040 [US3] Ensure public comments in `Source/Core/Public/Core/FMath.h` document right-handed coordinates, row-major matrices, tolerance rules, and invalid numeric input expectations
- [X] T041 [US3] Ensure public comments in `Source/Core/Public/Core/FMatrix4x4.h`, `Source/Core/Public/Core/FQuat.h`, and `Source/Core/Public/Core/FTransform.h` document multiplication, transformation, inverse, and composition semantics
- [X] T042 [US3] Ensure public comments in `Source/Core/Public/Core/FColor.h`, `Source/Core/Public/Core/FBox.h`, `Source/Core/Public/Core/FSphere.h`, and `Source/Core/Public/Core/FPlane.h` document conversion, validity, containment, and classification semantics
- [X] T043 [US3] Run `scons` and `Build/Mac/Debug/Tests/StonerTest` locally, then fix any US3 consistency failures in `Source/Core/Public/Core/` or `Tests/CoreMathTests.cpp`

**Checkpoint**: All user stories should pass locally and be ready for cross-platform verification.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup and acceptance validation across all stories.

- [X] T044 Verify public names follow UE5-style `F` prefixes in `Source/Core/Public/Core/FMath.h`, `Source/Core/Public/Core/FVector2.h`, `Source/Core/Public/Core/FVector3.h`, `Source/Core/Public/Core/FVector4.h`, `Source/Core/Public/Core/FMatrix4x4.h`, `Source/Core/Public/Core/FQuat.h`, `Source/Core/Public/Core/FTransform.h`, `Source/Core/Public/Core/FColor.h`, `Source/Core/Public/Core/FBox.h`, `Source/Core/Public/Core/FSphere.h`, and `Source/Core/Public/Core/FPlane.h`
- [X] T045 Verify no Core math public header includes RHI, Backend, Renderer, Application, platform windowing, graphics API, or physics headers in `Source/Core/Public/Core/`
- [X] T046 Verify `specs/004-core-math-library/contracts/core-math-api.md` is satisfied by the implemented headers in `Source/Core/Public/Core/`
- [X] T047 Review `Tests/CoreMathTests.cpp` to ensure every math category from `specs/004-core-math-library/spec.md` has normal, boundary, and invalid-input coverage where applicable
- [X] T048 Run the quickstart build and verification flow from `specs/004-core-math-library/quickstart.md`
- [X] T049 Run `scons` from the repository root and confirm `Build/Mac/Debug/Tests/StonerTest` exits with code 0 on macOS
- [X] T050 Update `doc/roadmap.md` Phase 003 status only after implementation and verification are complete

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - blocks all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational phase completion
- **User Story 2 (Phase 4)**: Depends on Foundational phase completion and uses `FMath`/`FVector3` behavior from US1
- **User Story 3 (Phase 5)**: Depends on US1 and US2 completion because it validates the complete public Core math surface
- **Polish (Phase 6)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: MVP. Establishes stable Core spatial values and scalar math utilities.
- **User Story 2 (P2)**: Adds color and geometry values; depends on vector and tolerance behavior from US1.
- **User Story 3 (P3)**: Validates aggregate include, isolation, documentation, diagnostics, and cross-platform consistency after US1 and US2.

### Within Each User Story

- Tests must be written before matching implementation tasks.
- Header-only value implementations in different files can proceed in parallel after their tests exist.
- `FMatrix4x4`, `FQuat`, and `FTransform` should be implemented after vector and `FMath` behavior is available.
- `FBox`, `FSphere`, and `FPlane` should be implemented after `FVector3` and tolerance helpers are available.
- Build and verification tasks follow implementation tasks.

### Parallel Opportunities

- T005 and T006 can run in parallel because they create different header groups.
- T017 through T020 can run in parallel after US1 tests are written because they modify different public headers.
- T030 through T033 can run in parallel after US2 tests are written because they modify different public headers.
- T040 through T042 can run in parallel after implementation stabilizes because they document different header groups.
- Polish validation tasks T044 through T047 can be split across reviewers after all story phases pass.

---

## Parallel Example: User Story 1

```bash
Task: "Implement FMath constants and scalar helpers in Source/Core/Public/Core/FMath.h"
Task: "Implement FVector2 value type and operations in Source/Core/Public/Core/FVector2.h"
Task: "Implement FVector3 value type, dot product, cross product, and safe normalization in Source/Core/Public/Core/FVector3.h"
Task: "Implement FVector4 value type and operations in Source/Core/Public/Core/FVector4.h"
```

---

## Parallel Example: User Story 2

```bash
Task: "Implement FColor float and byte-channel behavior in Source/Core/Public/Core/FColor.h"
Task: "Implement FBox empty/default state, construction, point addition, box combination, containment, center, and extent behavior in Source/Core/Public/Core/FBox.h"
Task: "Implement FSphere construction, validity, radius handling, and containment behavior in Source/Core/Public/Core/FSphere.h"
Task: "Implement FPlane construction, validity, signed distance, and point classification behavior in Source/Core/Public/Core/FPlane.h"
```

---

## Parallel Example: User Story 3

```bash
Task: "Ensure public comments in Source/Core/Public/Core/FMath.h document right-handed coordinates, row-major matrices, tolerance rules, and invalid numeric input expectations"
Task: "Ensure public comments in Source/Core/Public/Core/FMatrix4x4.h, Source/Core/Public/Core/FQuat.h, and Source/Core/Public/Core/FTransform.h document multiplication, transformation, inverse, and composition semantics"
Task: "Ensure public comments in Source/Core/Public/Core/FColor.h, Source/Core/Public/Core/FBox.h, Source/Core/Public/Core/FSphere.h, and Source/Core/Public/Core/FPlane.h document conversion, validity, containment, and classification semantics"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational scaffolding and conventions
3. Write US1 tests in `Tests/CoreMathTests.cpp`
4. Implement `FMath`, `FVector2`, `FVector3`, `FVector4`, `FMatrix4x4`, `FQuat`, and `FTransform`
5. Stop and validate that Core spatial math can be used without higher-layer dependencies

### Incremental Delivery

1. Deliver US1 for shared spatial values and scalar helpers
2. Add US2 for colors and basic geometric primitives
3. Add US3 for aggregate include, isolation, diagnostics, and cross-platform consistency validation
4. Finish with polish checks against the spec, contract, and quickstart

### Solo Agent Strategy

Work sequentially by task ID except for explicitly marked parallel header implementation tasks. Run `scons` and `StonerTest` at each checkpoint before moving to the next story.

---

## Notes

- Keep all new Core public deliverables inside `namespace Stoner::Core`.
- Do not add dependencies outside the C++ standard library for this feature.
- Do not implement spatial acceleration structures, physics-specific math, camera systems, animation systems, or renderer-owned scene data.
- Mark tasks complete in this file only after the implementation and relevant verification for that task are done.
