# B07-S09 Evidence: Entity Slots And Components Verification

Step: `B07-S09`.

Finding verified:

- `CR001-B07-F005`: Mesh, light, and camera component mutation bypasses data
  validation.

Static verification evidence:

- `Source/Application/Private/FWorld.cpp:250` validates
  `Component.IsValid()` in shared mesh/light/camera add paths before storage.
- `Source/Application/Private/FWorld.cpp:277` validates
  `Component.IsValid()` in shared mesh/light/camera replace paths before
  overwrite.
- Both paths return `ESceneResult::InvalidComponentData` and emit
  `SCENE-COMPONENT-INVALID-DATA` before mutating the entity slot.

Regression evidence:

- `Tests/ApplicationSceneEcsTests.cpp:108` covers invalid mesh add without
  storage.
- `Tests/ApplicationSceneEcsTests.cpp:115` covers invalid light replacement
  without mutating the existing component.
- `Tests/ApplicationSceneEcsTests.cpp:122` covers invalid camera replacement
  without mutating the existing component.
- `Tests/ApplicationSceneEcsTests.cpp:267` and
  `Tests/ApplicationSceneEcsTests.cpp:280` confirm render collection ordering
  and reachable missing-transform diagnostics remain deterministic after moving
  invalid component rejection to mutation time.

Gate evidence:

- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T08:14:50+00:00`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b07-entity-components-fix-stonertest.txt`
  records the component validation regressions and render collection
  deterministic tests as passing.

Result:

- `CR001-B07-F005` is verified.
