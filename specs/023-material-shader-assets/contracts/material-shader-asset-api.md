# Contract: Material and Shader Asset API

## Dependency Boundary

All public types in this contract live in `Source/Asset/Public/Asset` and may
include Core and Asset headers only. `yyjson`, Renderer, RHI, Application,
Backend, filesystem paths, and native handles are private implementation
details or forbidden dependencies.

## Immutable Payload Types

The feature adds these `FAssetPayload` values:

| C++ type | Fixed Asset type | Purpose |
|---|---|---|
| `FShaderAsset` | `ShaderProgram` | Complete program authority |
| `FShaderSourceAsset` | `ShaderSource` | Exact UTF-8 source bytes |
| `FShaderPayloadAsset` | `ShaderPayload` | Exact precompiled target bytes |
| `FMaterialAsset` | `Material` | Base material definition |
| `FMaterialInstanceAsset` | `MaterialInstance` | One-parent typed overrides |

Payload construction is private to validated loaders. Public access is
read-only and value-based. No payload contains a registry pointer, execution
lease, parser DOM, native object, or mutable cache.

## Limits

`FMaterialShaderAssetLimits` exposes the finite positive defaults in
`data-model.md`. Construction rejects zero values, values that cannot be
represented by checked allocation arithmetic, and inconsistent aggregate
limits. A caller may provide stricter or explicitly larger values per request;
there is no unlimited sentinel.

## Source Pipeline

```cpp
struct FMaterialShaderLoadRequest
{
    FAssetId ExpectedId;
    const FAssetExtensionRegistry* Extensions;
    FMaterialShaderAssetLimits Limits;
    bool bLoadDependencies = true;
};

struct FMaterialShaderLoadResult
{
    EAssetResult Result;
    Core::TArray<Core::TSharedPtr<const FAssetPayload>> Payloads;
    Core::TArray<FAssetMetadata> Metadata;
    Core::TArray<FAssetDependency> Dependencies;
    Core::FString CanonicalDefinition;
    Core::uint64 RegistryRevision = 0;
    FAssetDiagnosticList Diagnostics;
};
```

`FMaterialShaderSourceLoader::Load` is synchronous, request-scoped, and
reentrant. It performs bounded read, strict parse, model construction,
validation, dependency extraction, optional dependency loading, canonical
write, then atomic import-output emission. `Payloads`, metadata, and
dependencies are either complete and mutually consistent or empty. This
operation never mutates `FAssetRegistry`.

The existing `IAssetImporter` adapter probes suffix plus bounded root schema.
Suffix is only a hint; unsupported content returns NotApplicable without
publication. A recognized malformed definition returns a stable failure and
does not fall through to a lower-priority importer.

## Dependency Loading

Each source or payload reference is resolved through `FAssetDispatch` under one
Feature 020 resolver execution lease. The loader:

1. validates the typed ID and relative locator;
2. enforces per-file and aggregate limits before allocation;
3. reads exact bytes;
4. verifies SHA-256 and declared metadata;
5. applies UTF-8 source or SPIR-V structural validation;
6. constructs an immutable typed payload.

Two references to the same typed identity and version share one request-local
result. Conflicting locator, digest, or metadata declarations fail the complete
request. Feature 023 adds no process-wide dependency cache.

## Shader Selection

```cpp
struct FShaderTargetRequest
{
    EShaderBackendFamily Backend;
    Core::TArray<Core::FString> AcceptableProfiles;
    FShaderPermutationKey Permutation;
};

EAssetResult SelectShaderProgram(
    const FShaderAsset& Program,
    const FShaderTargetRequest& Request,
    const IShaderPayloadLookup& Payloads,
    FSelectedShaderProgram& OutSelection,
    FAssetDiagnosticList* Diagnostics);
```

Selection is deterministic and side-effect free. Profiles are evaluated in
caller order within exactly one backend. The first profile with one complete
stage set wins; ambiguity at that profile fails. No source compilation,
cross-profile mixing, cross-backend fallback, or registration-order tie-break
is allowed.

## Material Resolution

```cpp
EAssetResult ResolveMaterial(
    const FAssetId& MaterialOrInstance,
    const IMaterialAssetLookup& Lookup,
    const FMaterialShaderAssetLimits& Limits,
    FResolvedMaterialAsset& OutMaterial,
    FAssetDiagnosticList* Diagnostics);
```

The lookup exposes immutable Material and MaterialInstance payloads by typed
identity and version. Resolution checks the visited identity before every
parent fetch, enforces depth 64 by default, finds one root Material, validates
override names/types, and applies overrides root-to-leaf. Success returns one
flattened `FResolvedMaterialAsset`; no Asset or Renderer parent pointer escapes.

## Result and Diagnostic Stability

`EAssetResult` is extended with InvalidDefinition, UnsupportedSchema,
UnknownRequiredExtension, DefinitionLimitExceeded, DependencyMismatch,
InvalidShaderProgram, TargetUnavailable, AmbiguousTarget,
InvalidMaterialAsset, and InvalidInstanceChain. Renderer conversion failures
use existing `EMaterialResult` and `FMaterialDiagnosticLog`.

Diagnostics carry stable stage, subject, field/index, result, reason token,
actual value, and configured limit. They omit absolute paths, source lines,
payload bytes, thread IDs, addresses, timings, and raw third-party errors.

## Registry Publication

```cpp
class FMaterialShaderImportService
{
public:
    static FMaterialShaderLoadResult ImportAndRegister(
        const FAssetExtensionRegistry& Extensions,
        FAssetRegistry& Registry,
        const FAssetImportRequest& Request);
};
```

The service first obtains a complete loader/importer result without mutating
the Registry. It then creates one `FAssetMutationBatch` containing every
metadata output and calls `FAssetRegistry::Apply` exactly once. Success reports
the resulting Registry revision. Load, validation, batch-construction, or
`Apply` failure returns no usable payload result and leaves the prior Registry
snapshot/revision unchanged.

## Threading and Lifetime

- immutable payloads and completed selections support concurrent readers;
- one request has no mutable global state;
- extension/importer registrations obey Feature 020 execution leases;
- replacing Registry metadata never mutates an existing payload or selection;
- cancellation and asynchronous request handles remain Feature 026 work.
