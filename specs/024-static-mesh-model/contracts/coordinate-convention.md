# Contract: Coordinate Convention Migration

## Active Convention

After Migration M0, all active Core, Asset, Application, Renderer, RHI defaults,
Backend adapters, shaders, demos, tests, and validation evidence use:

```text
Identity: UnrealLH_ZUp_XForward_YRight_Meters_CW
World: left-handed interpretation
Forward: +X
Right: +Y
Up: +Z
Linear unit: meter
Canonical front face: clockwise
Clip depth: [0, 1]
```

This is not a runtime-selectable setting.

## Preserved Algebra

- `FVector3::UnitX().Cross(FVector3::UnitY()) == FVector3::UnitZ()`.
- Quaternion multiplication remains Hamilton multiplication.
- `FQuat::FromAxisAngle(+Z, +pi/2)` rotates +X to +Y.
- Matrices remain row-major in CPU storage.
- Matrix/vector operations use column vectors.
- `FTransform` applies scale, then rotation, then translation.
- Normal transformation remains inverse-transpose.

Descriptions that call these values "right-handed" are migrated, but the
component formulas above are not inverted.

## View And Projection

- A camera with identity world rotation looks along world +X.
- World +Y maps to view right; world +Z maps to view up.
- Renderer owns one canonical world-to-view/projection builder.
- StandardZ and ReversedZ both use `[0,1]` normalized clip depth.
- Transparent sort and light-volume culling use named view-depth accessors, not
  a hard-coded world or view component.

## Rasterization

- `FRHIRasterizerState` defaults to clockwise front face.
- A backend may adapt viewport orientation, but front-face parity is applied
  exactly once.
- Negative determinant transforms invert draw parity exactly once.
- glTF basis reflection is import parity; node/world negative scale is
  instance parity. They are tracked separately.

## Shader Matrix Packing

- CPU matrix bytes may not be copied into shader storage without an explicit
  packing function.
- A native probe using translation, rotation, and non-uniform scale must match
  CPU reference output.
- Identity-only validation is insufficient.

## Migration Evidence

The gate requires:

1. basis and positive-yaw Core tests;
2. transform compose, inverse, hierarchy, and preserve-world reparent tests;
3. +X camera forward, depth, transparency, and culling tests;
4. clockwise/negative-scale raster tests;
5. normal inverse-transpose and tangent parity tests;
6. non-symmetric CPU-to-shader matrix readback;
7. updated Feature 018 visible evidence;
8. updated Feature 019 native readback;
9. Windows, macOS, and Linux CI.

Features 004, 017, 018, and 019 receive dated migration amendments. Their
original decisions remain visible as historical evidence.
