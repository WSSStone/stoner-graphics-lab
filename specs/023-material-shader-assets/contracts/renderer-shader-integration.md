# Contract: Renderer and Native Shader Integration

## Ownership

| Responsibility | Owner |
|---|---|
| Identity, source schema, typed dependencies, validation, instance flattening | Asset |
| Conversion to Feature 014 records and RHI descriptions | Renderer |
| RHI shader-module contract and validation vocabulary | RHI |
| Native SPIR-V module/pipeline creation and rollback | Vulkan Backend |
| Resolver mount, synchronous load, selection, and snapshot wiring | Application/Demo |
| Content staging beside build/test outputs | SCons helper |

No Backend or RHI header includes Asset. Asset never includes Renderer or RHI.

## Shader Snapshot Conversion

```cpp
struct FShaderAssetConversionRequest
{
    const FSelectedShaderProgram* SelectedProgram;
};

struct FShaderAssetSnapshot
{
    Core::TArray<FAssetSourceVersionRecord> SourceManifest;
    FShaderTargetSelection SelectedTarget;
    Core::TArray<FShaderRecord> ShaderRecords;
    Core::TArray<RHI::FRHIShaderModuleDesc> ModuleDescriptions;
};
```

`ConvertShaderAsset` validates the complete selected stage set, copies all
strings, interfaces, words, identities, and versions, and returns a fully owned
immutable snapshot. Failure leaves the output unchanged and registers nothing.
`FRHIShaderModuleDesc::Bytecode.Words` is owned by the completed snapshot and
remains valid for at least the duration of module creation.

`FShaderLibrary` gains:

```cpp
EMaterialResult RegisterShaderRecords(
    std::span<const FShaderRecord> Records,
    FMaterialDiagnosticLog* Diagnostics = nullptr);
```

The batch validates all keys and conflicts before changing the library. It
commits every record or none. The existing one-record method delegates to the
batch and remains source-compatible.

## Material Snapshot Conversion

```cpp
struct FMaterialAssetConversionRequest
{
    const FResolvedMaterialAsset* ResolvedMaterial;
    const FShaderAssetSnapshot* Shader;
};

struct FMaterialAssetSnapshot
{
    Core::TArray<FAssetSourceVersionRecord> SourceManifest;
    FMaterial Material;
    FMaterialResourceRequirement ResourceRequirement;
};
```

Both a base Material and a MaterialInstance are resolved by Asset before this
boundary. Renderer always receives one flattened value and constructs one
Feature 014 `FMaterial`; it never creates an `FMaterialInstance` or stores a
parent pointer. Shader identity, variant, required parameters, texture
references, domain/blend/render state, and effective values must be equivalent
to the legacy in-memory construction path.

Conversion merges manifests and rejects one identity carrying different
versions. The completed snapshot owns every value and remains usable after
Registry replacement. Explicit reconversion is the only update operation.

## Native Bytecode Inputs

Native helpers accept immutable RHI/Core values, not paths:

```cpp
struct FVulkanDeferredShaderSet
{
    FRHIShaderModuleDesc SurfaceVertex;
    FRHIShaderModuleDesc SurfaceFragment;
    FRHIShaderModuleDesc FullscreenVertex;
    FRHIShaderModuleDesc CompositionFragment;
    FRHIShaderModuleDesc DirectionalFragment;
    FRHIShaderModuleDesc PointVertex;
    FRHIShaderModuleDesc PointFragment;
    FRHIShaderModuleDesc SpotVertex;
    FRHIShaderModuleDesc SpotFragment;
};
```

Triangle execute/visible preparation similarly receive vertex and fragment
module descriptions. The exact public overload shape may use spans or a small
pair struct, but it must retain the following invariants:

- no Content path, resolver, Asset ID, or Asset include enters Backend;
- the caller owns byte arrays until the native call has copied/created modules;
- Backend repeats RHI/SPIR-V validation and reports native failures normally;
- one stage failure destroys all modules created by that transaction;
- deterministic/failure-injection behavior remains equivalent.

`--shader-dir` may remain as an Application resolver mount option for
compatibility. It no longer crosses the Renderer/RHI/Backend boundary.

## Controlled Native Session Split

Feature 023 performs the affected portion of CR001-B09-F005:

1. remove shader file acquisition from
   `FVulkanNativeOffscreenSession::Execute`;
2. pass one immutable shader set into `Execute`;
3. isolate shader-module creation and rollback in a focused private operation;
4. keep command, attachment, pipeline, readback, and teardown behavior
   unchanged.

Further decomposition of the native session is deferred to the pre-Feature 027
gate and must not be hidden inside this migration.

## Content Staging

One reusable SCons helper stages declared Content files while preserving their
repository-relative layout. Demo and native tests call that helper instead of
maintaining independent shader copy lists. The helper:

- declares every source file explicitly to SCons;
- copies bytes without compilation or rewriting;
- makes missing/duplicate destinations fail deterministically;
- does not generate manifests, packages, DDC keys, or cooked assets.

Repository verification proves six program definitions cover the 11 GLSL and
11 SPIR-V files and that production/native code has no direct shader file read
below Application composition.

## Compatibility and Rollback

The migration is additive until all repository call sites use snapshots:

1. add Asset definitions/loaders and Renderer adapters;
2. prove equivalence against Feature 014 records;
3. add byte-input native overloads and migrate tests/Demo;
4. remove path-input Backend overloads and duplicate copy lists;
5. run deterministic and native regression gates.

No compatibility overload may translate a path inside Backend. At every step,
failure leaves the prior library, snapshot, and native resource state intact.
