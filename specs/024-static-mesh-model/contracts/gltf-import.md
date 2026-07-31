# Contract: Bounded glTF/GLB Import

## Registration And Probe

The importer registers for `.gltf`, `.glb`, and matching content signatures
through the existing extension registry. Probe confidence is deterministic:

1. valid GLB magic plus compatible descriptor;
2. bounded JSON prefix with glTF asset/version shape;
3. extension/format hint only.

Probe never reads more than the existing maximum probe prefix.

## Request

```cpp
struct FStaticModelImportRequest
{
    FAssetImportRequest AssetRequest;
    Core::TSharedPtr<IAssetResolver> DependencyResolver;
    Core::TSharedPtr<const FStaticModelImportProfile> Profile;
};
```

The importer rejects a missing/invalid profile or resolver when dependencies
require one. It does not open paths directly.

## Accepted Scope

- glTF 2.0 JSON and GLB 2.0;
- scenes, nodes, local TRS/matrix, meshes, triangle primitives;
- indexed and non-indexed primitives;
- positions, normals, tangents, UV0 and UV1;
- normalized, interleaved, and sparse accessors;
- core metallic-roughness materials, alpha mode/cutoff, double-sided state;
- base-color, metallic-roughness, normal, occlusion, and emissive textures;
- embedded data URI, GLB BIN, and resolver-scoped external buffers/images.

## Ignored And Rejected Scope

Cameras, punctual lights, and animations may be ignored with normalized
inspection evidence because they do not become outputs in Feature 024.

Stable `Unsupported` diagnostics are returned for:

- non-triangle primitive modes;
- skins and morph targets that affect selected static geometry;
- mesh compression or required extensions without support;
- more than two required UV sets;
- FBX, OBJ, USD, or other source formats.

Malformed data returns `MalformedSource` or `MalformedContainer`; finite-limit
failures return `CapacityExceeded` (or the existing format-specific bounded
result); missing dependencies return `NotFound`; scope violations return
`AccessDenied`.

## Security And Bounds

Before parser traversal:

- validate source size and GLB header/chunk structure;
- use checked arithmetic for all offsets, lengths, alignments, strides, and
  aggregate sizes;
- install a capped parser allocator;
- reject embedded NUL, invalid UTF-8, invalid percent encoding, unsupported URI
  scheme, absolute-path escape, parent traversal, alias escape, and dependency
  recursion;
- enforce all `FStaticModelImportLimits`.

`cgltf_validate()` and Khronos Validator are secondary conformance evidence,
not safety boundaries.

## Accessors

- Validate component type, accessor type, count, offset, stride, alignment, and
  buffer range before decode.
- Sparse indices are strictly increasing, unique, and in range.
- Normalized integer conversion follows glTF 2.0.
- Decoded geometry values must be finite.
- Indices normalize to the narrowest lossless uint16/uint32 representation.
- Non-indexed primitives receive sequential indices.

## Coordinate And Attribute Normalization

- Position/vector basis: `(X, Y, Z)engine = (Z, -X, Y)gltf`.
- Node matrix: `C * Mgltf * inverse(C)`.
- Source index order remains unchanged.
- Tangent W flips once for basis reflection.
- Missing normals use deterministic flat-normal vertex splitting under the
  default policy.
- Missing tangents use pinned MikkTSpace only when required by a normal map and
  valid selected UV set.
- Strict policy rejects missing required streams.

## Package Output

The importer may emit:

- one static model per scene;
- all static meshes, including unreferenced meshes;
- mapped Material/MaterialInstance schema-v2 assets;
- Image/Texture assets through Feature 021 contracts;
- complete metadata, dependency, and source-version evidence.

All output is assembled and validated in scratch state. Success returns the
complete deterministic output array sorted by asset identity. Any failure
returns an empty output array and no registry mutation.
