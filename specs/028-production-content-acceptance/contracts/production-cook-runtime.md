# Production Cook and Runtime Contract

## Purpose

Defines the end-to-end contract from an admitted production root to a strict
cooked runtime closure. This contract is implemented by existing Asset and
AssetCooker surfaces with the Feature 028 KTX2 producer-selection correction.

## Inputs

- Canonical corpus manifest and verified package directory.
- One explicit `StaticModel` root AssetId per run; medium acceptance enumerates
  every accepted package root rather than substituting one aggregate root.
- Ordered source roots containing the package and repository-owned shader assets.
- Exact `FAssetTargetProfileEvidence` and target profile file.
- Empty or existing immutable DDC root, publication root, and scratch root.

## Preconditions

1. Corpus verification passes before source discovery.
2. Every source root is bounded and canonical; explicit root mode is used.
3. The default glTF material mapping shader dependency resolves.
4. Profile producer settings include every selected producer.
5. The prior published generation, if any, remains untouched until atomic
   publication succeeds.

## Required Producer Selection

| Input payload | Required producer | Required output |
|---|---|---|
| `FImageAsset` | Existing cooked image producer when independently rooted | Existing typed cooked image envelope |
| `FTextureAsset` | `cooker.ktx2` | `FKTX2TextureArtifact` / `stoner.ktx2` evidence |
| Material/instance/shader asset | Existing material/shader producer | Target-valid typed payload |
| Vulkan SPIR-V selected for Metal | `cooker.metal-shader` | Offline metallib payload |
| Static mesh/model asset | Existing static-model producer | Typed cooked mesh/model payload |

The target decision and producer ID are both verified. A generic serialized
`FTextureAsset` is not accepted as KTX2 even when its decision names a
compressed format.

## Publication Postconditions

- One generation contains the requested root and complete required dependency
  closure.
- Every manifest entry has a matching envelope, type, AssetId, version, target,
  producer, derived-key evidence, and payload digest.
- Generation identity and canonical manifest are independent of host path,
  enumeration order, timing, locale, PID, or thread completion order.
- For every accepted root and required target, clean equivalent cooks match;
  unchanged warm cook reuses 100% of eligible entries and yields an equivalent
  normalized generation.
- Failure preserves the prior valid generation and leaves no partial current
  pointer.

## Strict Runtime Contract

1. Configure `FAssetManager` in strict cooked mode with one publication root,
   exact target profile, and separate writable lease-coordination root.
2. Bind and validate one immutable generation before requesting the root.
3. Disable or unmount package source roots for the acceptance run.
4. Request the typed model root and wait for all required dependencies.
5. Source resolver, importer, authoring decoder, source fallback, and Tools call
   counters MUST remain zero from generation bind through final release.
6. Return either one complete production closure or a stable failure; never
   expose a partially complete root.

Medium acceptance applies this complete strict-runtime sequence and the full
semantic-equivalence matrix independently to every accepted package root.

## Semantic Equivalence Matrix

| Payload family | Required comparison |
|---|---|
| Identity/metadata/dependency | Exact canonical fields, roles, and ordering |
| Static model | Exact scene/root, hierarchy, transforms, primitive/material associations |
| Static mesh | Exact topology/index values; normalized finite vertex attributes/tangents/bounds within existing import tolerance |
| Material/instance | Exact blend/render state, typed parameters, resource refs, shader identity |
| Shader | Exact interface/stage/permutation intent and correct target payload selection; representation may differ |
| Image/texture | Exact dimensions, mip count/extent, semantic/color space/swizzle intent; decoded/transcoded values use declared semantic tolerance |

## Failure Categories

`CorpusRejected`, `SourceChanged`, `GraphIncomplete`, `ProducerMismatch`,
`CookFailed`, `PublicationFailed`, `GenerationInvalid`, `WrongTarget`,
`PayloadCorrupt`, `TypeMismatch`, `DependencyMissing`, `SourceFallbackObserved`,
`Cancelled`, and `Shutdown`.

Each failure records one stable stage, subject AssetId/path token, reason token,
and reproduction profile. Absolute paths and native identifiers are forbidden.
