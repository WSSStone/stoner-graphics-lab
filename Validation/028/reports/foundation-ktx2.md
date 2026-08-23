# Feature 028 Foundational KTX2 Gate

Captured on 2026-08-22 from branch
`028-production-content-acceptance` after completing T008-T018.

## Delivered Contract

- `FTextureAsset` selects `cooker.ktx2`; other payload families retain their
  existing producer selection.
- The selected KTX2 cook parameters and texture semantic, color-space, mip,
  target-capability, and fallback decisions participate in deterministic
  derived-key evidence.
- The AssetCooker publishes an `FKTX2TextureArtifact` inside the typed
  `SGCOOK01` envelope. Strict loading reopens the KTX2 body and rejects metadata
  that does not agree with the actual container.
- Color, tangent-space normal, and data textures retain distinct semantics;
  normal and data textures cannot select an sRGB decision.
- All shipping Vulkan and Metal profiles declare canonical KTX2 producer
  settings and capability-correct compressed or portable fallback policy.

## Focused Evidence

The Debug production-texture suite passed eight selection, publication, DDC,
corruption, and strict-envelope tests. It includes a clean cook, unchanged warm
reuse, targeted DDC corruption quarantine/rebuild, and rejection of a generic
texture producer impersonating the selected KTX2 path.

The following strict Release suites passed:

```text
asset-cooker-production-texture
asset-cooker-target-profile
asset-cooker-profile-invalidation
asset-manager-equivalence
asset-cooker-determinism
```

They verify all five shipping profiles, producer/profile projection isolation,
representative Feature 021-024 cooking, development-versus-strict semantic
equivalence, and zero source fallback in strict mode.

## Regression Gates

| Gate | Result |
|---|---|
| `Build/Mac/Debug/Tests/StonerTest --suite asset` | PASS |
| `conda run -n godot scons -j8 config=release strict=1` | PASS |
| Strict Release focused suites listed above | PASS |
| `conda run -n godot python Tests/verify_architecture.py` | PASS |

The local Apple toolchain emitted its existing `confstr()` temporary-directory
warning while compiling, but strict project warnings remained errors and the
build completed successfully.
