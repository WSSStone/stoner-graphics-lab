# B07-S08: Entity Slots And Components Fix

## Scope

Fixed `CR001-B07-F005`.

Code changed:

- `Source/Application/Private/FWorld.cpp`
- `Tests/ApplicationSceneEcsTests.cpp`

## Fixes

### `CR001-B07-F005`

- Mesh, light, and camera component add paths now validate
  `Component.IsValid()` before mutating the entity slot.
- Mesh, light, and camera component replace paths now validate
  `Component.IsValid()` before overwriting an existing component.
- Invalid component payloads now return `ESceneResult::InvalidComponentData`
  and emit `SCENE-COMPONENT-INVALID-DATA`, matching the existing transform
  component behavior.
- Existing valid component state is preserved when an invalid replacement is
  attempted.

## Regression Coverage

Added tests for:

- invalid mesh add being rejected without storing a mesh component;
- invalid light replacement being rejected without mutating the existing light;
- invalid camera replacement being rejected without mutating the existing
  camera.

Updated render collection tests so invalid light and camera component data are
no longer expected to reach collection-time rejection. After this fix, invalid
mesh/light/camera payloads are rejected at the component mutation boundary;
render collection still covers reachable missing-transform rejection.

## Verification

- `git diff --check -- Source/Application/Private/FWorld.cpp Tests/ApplicationSceneEcsTests.cpp`: passed.
- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T08:09:47+00:00`.

## Commit

- `c6dd0a6`: `fix(application): validate scene component mutations`
