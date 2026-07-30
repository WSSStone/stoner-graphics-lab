# Contract: Stoner KTX2 Portable Texture Profile v1

## Profile Identifier

`stoner.ktx2.portable.v1`

The identifier versions all normative rules in this document. Changing encoder
parameter mappings, required metadata, semantic interpretation, or byte
canonicalization requires a new profile or producer version.

## Texture Scope

| KTX2 field | Required value |
|---|---|
| pixelWidth | 1..configured MaxDimension |
| pixelHeight | 1..configured MaxDimension |
| pixelDepth | 0 |
| layerCount | 0 |
| faceCount | 1 |
| levelCount | explicit source mip count, never 0 |
| typeSize | required KTX2 value for selected storage |

Cubemaps, arrays, volumes, sparse/partial mip payloads, and generated-at-load
level count zero are invalid for this profile.

Logical level index zero is the base mip. Extents follow
`max(1, base >> level)` and exactly match the Feature 021 texture.

## Compression and Format

### Basis artifacts

- ETC1S uses BasisLZ supercompression and the required ETC1S DFD/SGD.
- UASTC uses the required UASTC DFD and no additional zlib/zstd layer.
- `vkFormat` is undefined as required for Basis payloads.
- ETC1S is allowed only for LDR Color.
- UASTC is allowed for LDR Color and Normal, and for Data only with explicit
  lossy permission.

### Uncompressed artifacts

The private container writer maps Feature 021 layouts to standards-defined KTX2
formats:

| Asset layout | KTX2 storage |
|---|---|
| R8 UNorm | R8 UNorm |
| RG8 UNorm | RG8 UNorm |
| RGB8 UNorm | RGB8 UNorm |
| RGBA8 UNorm | RGBA8 UNorm |
| RGB32F | RGB32 float |
| RGBA16F | RGBA16 float |
| RGBA32F | RGBA32 float |

Color sRGB chooses the corresponding sRGB storage/DFD where defined. Normal and
Data are always linear. HDR is always uncompressed float. No hidden RGBA
expansion occurs in Asset.

## Data Format Descriptor

- transfer function is sRGB only for `Semantic=Color` and
  `ColorSpace=SRGB`;
- all other content declares linear transfer;
- alpha is straight/unassociated; premultiplied DFD flags are forbidden;
- channel/sample information must agree with the source layout and Basis model;
- normal/data semantics are additionally recorded in Stoner metadata because
  DFD alone does not define engine semantic use.

DFD contradictions are `MalformedContainer`, not conversion requests.

## Required Key/Value Metadata

Keys are inserted in the following lexicographic order and appear exactly once:

1. `KTXorientation`
2. `KTXwriter`
3. `stoner.alphaMode`
4. `stoner.assetId`
5. `stoner.channelCount`
6. `stoner.contentDigest`
7. `stoner.cookRevision`
8. `stoner.mipPolicy`
9. `stoner.portableProfile`
10. `stoner.semantic`
11. `stoner.sourceDigest`

Values:

| Key | Canonical value |
|---|---|
| KTXorientation | NUL-terminated `rd` |
| KTXwriter | NUL-terminated `StonerGraphicsLab/022-v1` |
| stoner.alphaMode | `none` or `straight` |
| stoner.assetId | canonical Feature 020 AssetId string |
| stoner.channelCount | original source layout channel count, `1` through `4` |
| stoner.contentDigest | 64-character lowercase SHA-256 |
| stoner.cookRevision | 64-character lowercase SHA-256 |
| stoner.mipPolicy | `full-chain` or `base-only` |
| stoner.portableProfile | `stoner.ktx2.portable.v1` |
| stoner.semantic | `color`, `normal`, or `data` |
| stoner.sourceDigest | 64-character lowercase SHA-256 |

Unknown nonreserved keys may be retained only if a future profile explicitly
permits them. Feature 022 writer emits none. Duplicate, malformed UTF-8,
noncanonical spelling, embedded NUL before the terminator, or contradictory
required values fail validation.

No value contains source path, absolute path, timestamp, username, hostname,
compiler name, registration order, pointer, thread ID, or runtime target
preference.

## Cook Revision Serialization

The cook revision is SHA-256 over length-prefixed canonical fields in this exact
order:

1. profile identifier;
2. producer version;
3. encoder module SHA-256;
4. source Texture AssetId;
5. source digest;
6. Feature 021 texture content digest;
7. semantic, color space, alpha, origin, and mip policy enum names;
8. resolved compression policy;
9. quality profile;
10. explicit lossy-data flag;
11. ordered mip count, extents, source formats, row pitches, byte counts, and
    mip bytes;

Integers use fixed-width little-endian encoding. Enum values serialize by fixed
contract tokens, not C++ ordinal. Strings use UTF-8 byte length plus bytes.
Container `ArtifactDigest` is a separate SHA-256 over final KTX2 bytes.

Runtime target profiles and device capabilities do not enter cook revision.
Operational safety limits also do not enter cook revision when the same source
is accepted, because they do not alter output bytes.

## Canonical Encoder ABI

The checked-in WebAssembly module:

- is identified by exact SHA-256 and source revision;
- exports allocation/free, version, and one cook entry point;
- accepts a bounded canonical settings block plus tightly packed ordered raw
  mip bytes;
- returns either one complete KTX2 byte range and normalized status or no
  output;
- uses one logical thread and no SIMD;
- imports no clock, randomness, filesystem, environment, network, or host
  logging;
- cannot access memory outside its configured linear-memory budget.

The host validates module version/hash before first use and validates every
returned range before copy. Any module trap, ABI mismatch, over-budget growth,
invalid output range, or nonzero codec status becomes `CookFailure`.

Portable profile v1 fixes quality mappings inside that request:

| Policy | Balanced | High |
|---|---|---|
| ETC1S | quality 192, compression level 2 | quality 255, compression level 2 |
| UASTC | level 2, RDO disabled | level 3, RDO disabled |

All four Basis combinations use one logical worker. The host MUST NOT rewrite a
successful compressed KTX2 byte range; it only preflights, reopens, validates,
and hashes it. The host canonical writer is used for uncompressed KTX2 only.

## Reopen Requirement

A cook succeeds only after:

1. structural preflight passes;
2. libktx opens and validates the produced bytes;
3. normalized info agrees with the source contract and cook settings;
4. exact artifact SHA-256 is computed;
5. the caller-visible artifact and generic byte result agree.

The external `ktx validate` oracle is a CI acceptance gate, not part of the
production Asset call.
