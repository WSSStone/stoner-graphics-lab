# B07-S11: Hierarchy, Reparenting, And Destruction Fix

## Scope

Fixed `CR001-B07-F006`.

Code changed:

- `Source/Application/Public/Application/FWorld.h`
- `Source/Application/Private/FWorld.cpp`
- `Tests/ApplicationSceneEcsTests.cpp`

## Fixes

### `CR001-B07-F006`

- Added an internal hierarchy-world transform helper that allows
  transformless hierarchy nodes to behave as identity local transforms while
  still inheriting transformed ancestors.
- Kept the public `TryGetWorldTransform` behavior strict: the queried entity
  itself must still have a transform component.
- Updated reparent preservation calculations to use hierarchy-world parent
  transforms, so preserve-world/default reparenting through transformless
  groups no longer ignores transformed ancestors.
- Preserved failure behavior when a parent hierarchy has transform state but
  cannot produce a representable TRS world transform.

## Regression Coverage

Added tests for:

- `Root(transform) -> Group(no transform) -> Child(transform)` world transform
  propagation;
- default preserve-world reparenting from one transformless group to another
  under transformed ancestors.

Existing tests continue covering direct transformed parent-child propagation,
topological order, cycle rejection, preserve-local reparenting, unrepresentable
TRS rejection, and recursive destruction.

## Verification

- `git diff --check -- Source/Application/Public/Application/FWorld.h Source/Application/Private/FWorld.cpp Tests/ApplicationSceneEcsTests.cpp`: passed.
- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T08:28:04+00:00`.

## Commit

- `00d3ffc`: `fix(application): propagate transforms through hierarchy groups`
