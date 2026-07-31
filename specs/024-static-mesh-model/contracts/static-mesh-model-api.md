# Contract: Static Mesh And Model Asset API

## Public Asset Types

```cpp
namespace Stoner::Asset
{

class FStaticMeshAsset final : public FAssetPayload
{
public:
    static EAssetResult CreateValidated(
        FStaticMeshAssetDesc Desc,
        FStaticMeshAsset& OutAsset,
        FAssetDiagnosticList* Diagnostics = nullptr);

    Core::FString GetAssetType() const override; // "StaticMesh"
    const FStaticMeshAssetDesc& GetDesc() const noexcept;
};

class FStaticModelAsset final : public FAssetPayload
{
public:
    static EAssetResult CreateValidated(
        FStaticModelAssetDesc Desc,
        FStaticModelAsset& OutAsset,
        FAssetDiagnosticList* Diagnostics = nullptr);

    Core::FString GetAssetType() const override; // "StaticModel"
    const FStaticModelAssetDesc& GetDesc() const noexcept;
};

}
```

Public headers depend only on Asset and Core.

## Mesh Validation

`CreateValidated` succeeds only when:

- at least one primitive exists;
- positions and canonical normals are finite and count-aligned;
- optional tangent/UV streams are absent or count-aligned;
- indices are a non-empty triangle list and are in range;
- material slot indices are valid;
- stable primitive and slot keys are unique;
- primitive and aggregate bounds are finite and enclosing;
- dependency/source manifests are normalized and complete;
- schema/profile/convention versions are recognized.

Failure clears the output to its default empty state and emits stable
field-qualified diagnostics.

## Model Validation

`CreateValidated` succeeds only when:

- scene key and node keys are valid and unique;
- each child index is in range and unique within its parent;
- each node has at most one parent;
- the hierarchy is acyclic and within configured depth;
- roots exactly match parentless nodes;
- local transforms are finite;
- mesh references and dependency records agree;
- aggregate bounds enclose all transformed mesh bounds.

## Immutability And Ownership

- Validated assets expose const descriptions only.
- Source/parser buffers are not retained.
- Payloads contain no RHI, Renderer, Backend, filesystem, or parser-native
  types.
- Asset versions cover canonical payload, source manifest, dependency versions,
  parser/generator versions, and import profile.

## Identity

- `extras.stonerAssetId` becomes `key.<normalized-value>`.
- Fallback identities use `idx.<typed-structural-path>`.
- Display names never contribute.
- Explicit keys must be unique within output type.
- One model asset is emitted per source scene.
- Unreferenced meshes still emit static mesh assets.
