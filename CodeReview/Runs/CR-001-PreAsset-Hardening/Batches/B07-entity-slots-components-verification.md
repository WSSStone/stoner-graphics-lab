# B07-S09: Entity Slots And Components Verification

## Scope

Verified `CR001-B07-F005`.

Files checked:

- `Source/Application/Private/FWorld.cpp`
- `Tests/ApplicationSceneEcsTests.cpp`

## Verification

### `CR001-B07-F005`

- Transform component mutation already validated `Component.IsValid()` before
  storage.
- Mesh, light, and camera component add/replace mutation paths now share the
  same validation behavior through `SG_SCENE_COMPONENT_METHODS`.
- Invalid mesh add returns `ESceneResult::InvalidComponentData` and leaves no
  mesh component stored.
- Invalid light and camera replacements return
  `ESceneResult::InvalidComponentData` and preserve the existing valid
  component state.
- Render collection tests now only expect rejection diagnostics for reachable
  collected state; invalid light/camera payloads are rejected before collection.

## Gate Evidence

- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T08:14:50+00:00`.

## Result

`CR001-B07-F005` is verified.
