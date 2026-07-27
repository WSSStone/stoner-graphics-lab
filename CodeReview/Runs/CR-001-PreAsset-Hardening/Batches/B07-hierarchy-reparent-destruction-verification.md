# B07-S12: Hierarchy, Reparenting, And Destruction Verification

## Scope

Verified `CR001-B07-F006`.

Files checked:

- `Source/Application/Public/Application/FWorld.h`
- `Source/Application/Private/FWorld.cpp`
- `Tests/ApplicationSceneEcsTests.cpp`

## Verification

### `CR001-B07-F006`

- `FWorld` now separates public world-transform queries from internal
  hierarchy parent-chain propagation.
- Public `TryGetWorldTransform` still requires the queried entity to have a
  transform component, preserving render collection missing-transform
  rejection.
- Internal hierarchy propagation treats transformless intermediate hierarchy
  nodes as identity local transforms and continues inheriting transformed
  ancestors.
- Reparent preservation calculations use hierarchy-world parent transforms, so
  transformless grouping entities no longer hide ancestor transforms.
- Regression tests cover transform propagation and preserve-world reparenting
  through transformless groups.

## Gate Evidence

- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T08:31:12+00:00`.

## Result

`CR001-B07-F006` is verified.
