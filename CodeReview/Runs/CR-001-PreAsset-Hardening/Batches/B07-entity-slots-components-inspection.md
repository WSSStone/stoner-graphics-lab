# B07-S07: Entity Slots And Components Inspection

## Scope

Inspected Feature 017 entity handle identity, slot lifecycle, component
mutation, component validation, and existing scene ECS tests.

Production files read:

- `Source/Application/Public/Application/FEntity.h`
- `Source/Application/Public/Application/FWorld.h`
- `Source/Application/Private/FWorld.cpp`
- `Source/Application/Public/Application/FTransformComponent.h`
- `Source/Application/Private/FTransformComponent.cpp`
- `Source/Application/Public/Application/FMeshComponent.h`
- `Source/Application/Private/FMeshComponent.cpp`
- `Source/Application/Public/Application/FLightComponent.h`
- `Source/Application/Private/FLightComponent.cpp`
- `Source/Application/Public/Application/FCameraComponent.h`
- `Source/Application/Private/FCameraComponent.cpp`

Supporting files read:

- `Tests/ApplicationSceneEcsTests.cpp`
- `specs/017-scene-graph-ecs/spec.md`
- `specs/017-scene-graph-ecs/contracts/scene-graph-ecs-contract.md`

## Requirements Checked

- Feature 017 FR-001: world create/reset ownership.
- Feature 017 FR-002: entity handles include identity and generation validation
  so reused slots do not revive stale handles.
- Feature 017 FR-003: destroy recursively removes descendants and components.
- Feature 017 FR-004: supported component add/read/update/replace/remove.
- Feature 017 FR-005: duplicate, missing, invalid entity, and invalid component
  data outcomes are stable and do not corrupt prior state.
- Component Contract: add/replace of component data is subject to implementation
  validation.

## Finding

### `CR001-B07-F005`

`FWorld` validates transform components during add/replace, but the generic
mesh/light/camera component macro assigns component payloads directly. Mesh,
light, and camera types all expose `IsValid`, but invalid payloads can still be
stored as live world state.

Impact: component mutation can report `Success` where the contract requires
`InvalidComponentData`, leaving invalid mesh, light, or camera records for later
render collection or downstream code to reject.

Status: Accepted, S2.

## Non-Findings

- Destroying an entity builds subtree order, removes the root from parent/root
  lists, clears component flags/payloads, increments generation, and returns
  slots to the free list.
- Reused slots receive a different generation, and stale handles validate as
  `StaleEntity`.
- Duplicate transform add preserves the previous transform, and transform
  invalid data is rejected before mutation.
