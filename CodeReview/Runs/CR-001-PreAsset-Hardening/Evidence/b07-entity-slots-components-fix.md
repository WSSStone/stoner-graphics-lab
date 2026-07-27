# B07-S08 Evidence: Entity Slots And Components Fix

Step: `B07-S08`.

Finding fixed:

- `CR001-B07-F005`: Mesh, light, and camera component mutation bypasses data
  validation.

Resolution:

- Shared mesh/light/camera component add and replace implementations now reject
  invalid component payloads before storing them in an entity slot.
- Invalid payloads return `ESceneResult::InvalidComponentData` and add a
  `SCENE-COMPONENT-INVALID-DATA` diagnostic.
- Existing valid light and camera components remain unchanged after invalid
  replacement attempts.
- Render collection tests now assert the reachable missing-transform rejection;
  invalid light and camera collection diagnostics are no longer expected because
  those invalid components cannot be inserted through public mutation APIs.

Regression evidence:

- `[PASS] Scene mesh add rejects invalid component data without storing it`
- `[PASS] Scene light replace rejects invalid component data without mutating existing component`
- `[PASS] Scene camera replace rejects invalid component data without mutating existing component`
- `[PASS] Scene render collection produces deterministic accepted/rejected category ordering`
- `[PASS] Scene render collection diagnostics and dumps are byte-stable across repeated runs`

Gate evidence:

- `git diff --check -- Source/Application/Private/FWorld.cpp Tests/ApplicationSceneEcsTests.cpp`: passed.
- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed, with output captured in
  `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b07-entity-components-fix-stonertest.txt`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T08:09:47+00:00`.

Code commit:

- `c6dd0a6`: `fix(application): validate scene component mutations`
