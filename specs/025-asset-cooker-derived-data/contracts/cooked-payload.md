# Contract: Cooked Asset Payload v1

## File Identity

- Extension: `.sgasset`
- Magic: ASCII `SGCOOK01`
- Container version: 1
- Integer encoding: little-endian, independent of host and target CPU
- Text: canonical UTF-8 without NUL
- Maximum file size: request limit, never above compiled 1 GiB v1 maximum

## Envelope

The file is one variable header followed by one type-specific body. Every
integer read uses checked conversion and every length is validated before
allocation or offset addition.

| Order | Field | Encoding |
|---:|---|---|
| 1 | Magic | 8 bytes, `SGCOOK01` |
| 2 | ContainerVersion | uint16, value 1 |
| 3 | HeaderBytes | uint16, >= fixed prefix and <= file size |
| 4 | Flags | uint32; unknown required bits fail |
| 5 | AssetId | uint32 byte length + UTF-8 bytes |
| 6 | AssetType | uint16 byte length + UTF-8 canonical token |
| 7 | CodecId | uint16 byte length + UTF-8 canonical token |
| 8 | CodecVersion | uint32 |
| 9 | PayloadSchemaVersion | uint32 |
| 10 | BodyBytes | uint64 |
| 11 | BodyDigest | 32-byte SHA-256 |
| 12 | ReservedHeaderExtensions | exactly `HeaderBytes` bounded bytes |
| 13 | Body | exactly `BodyBytes` bytes; no trailing data |

`EnvelopeDigest` is SHA-256 over all bytes and is external evidence in DDC
metadata and the manifest. An envelope does not contain its own envelope digest.

## Validation Order

1. Enforce total-byte limit and minimum fixed prefix.
2. Validate magic, container version, flags, and checked header/body boundaries.
3. Validate UTF-8, canonical AssetId, type agreement, codec token, and revisions.
4. Require exact end of file and verify body SHA-256.
5. Dispatch to the registered codec by Asset type and codec revision.
6. Decode into scratch storage with type-specific limits.
7. Run the same complete Asset validator used by source import.
8. Publish an immutable payload only after every step succeeds.

Unknown container/codec/schema revisions fail closed. A registered codec may
declare explicit backward-compatible body revisions; silent best-effort parsing
is forbidden.

## Public Codec Facade

`Asset/FAssetCookContractCodec.h` exposes typed bounded parse, write, and
validate operations for target profiles, cooked envelopes, and manifests.
Offline Tools and later runtime consumers use only this public facade plus
registered `IAssetCooker`/`IAssetLoader` dispatch. Concrete yyjson and
type-specific codec headers remain Asset-private and expose no third-party or
native platform types.

## Codec IDs v1

| Asset family | Codec token | Body contract |
|---|---|---|
| Image | `stoner.image` | Typed metadata, mip descriptors, pixels |
| Texture | `stoner.texture` | Usage/semantic/sampler metadata and image or KTX2 dependency evidence |
| KTX2 texture artifact | `stoner.ktx2` | Semantic header plus exact validated KTX2 bytes |
| Shader source | `stoner.shader-source` | Canonical source metadata and bounded UTF-8/source bytes |
| Shader payload | `stoner.shader-payload` | Backend/profile/stage/entry/permutation metadata and exact binary bytes |
| Shader program | `stoner.shader-program` | Canonical Feature 023 definition and dependency references |
| Material | `stoner.material` | Canonical Feature 023 definition and dependency references |
| Material instance | `stoner.material-instance` | Canonical Feature 023 definition and parent/dependency references |
| Static mesh | `stoner.static-mesh` | Stream/index/primitive/slot/bounds/source records |
| Static model | `stoner.static-model` | Scene/node/transform/mesh/default-scene/source records |

All v1 codec bodies use fixed field order, explicit counts, and length-prefixed
arrays. No pointer, `sizeof(struct)`, compiler ABI, padding byte, native enum
width, or platform path is serialized.

## Semantic Equivalence

For each asset family, source-imported and cooked-loaded payloads compare using
the owning Asset type's normalized semantic model. Physical byte equality with
the in-memory source representation is not required. The comparison includes:

- canonical AssetId and payload type;
- payload schema and semantic fields;
- normalized direct dependencies and complete source-version records;
- image/texture color, usage, orientation, mip, and format meaning;
- shader target/stage/entry/permutation and exact source/payload bytes;
- material parameters, bindings, render state, parent/default behavior;
- mesh streams, indices, primitives, slots, bounds, coordinates, and versions;
- model scenes, topological node order, local transforms, mesh references, and
  default-scene behavior.

## Limits And Malformed Cases

Validation explicitly covers zero/truncated/extra bytes, integer overflow,
invalid UTF-8, non-canonical AssetId, type mismatch, unknown required flags,
wrong body digest, body count/size disagreement, duplicate semantic keys,
non-finite numeric fields, invalid indices/offsets, dependency mismatch, and
payload schema/codec incompatibility.
