# Feature 021 Image Fixture Manifest

The checked-in corpus is intentionally tiny, asymmetric, and deterministic.
Tests read these bytes directly; regeneration is optional and must not silently
replace a fixture whose digest or decoded facts change.

## Valid Corpus

| File | SHA-256 | Expected canonical facts |
|---|---|---|
| `hdr-rgb-3x2.hdr` | `a96c70b2f444da5764d021ca5df76f8d2a182e11dd2dbd8f18c5a857ae00f30c` | 3x2 HDR; linear; default RGBA16F; explicit RGBA32F/RGB32F supported |
| `jpeg-exif-o6-2x3.jpg` | `7db5d8efcc2c9b8738b45191da70e69774494361aa466e7c344c3f3e4eeadd79` | JPEG RGB; APP1 orientation 6; canonical top-left extent 3x2 |
| `jpeg-gray-5x3.jpg` | `f9ec68f377062ce3e1d250b00e714b87388f2224ee6017d336a483ede914ede1` | JPEG gray; 5x3; R8; sRGB default |
| `jpeg-rgb-3x5.jpg` | `285f7c2212e8fac6daf291d149a1712d56bbef60047abc7ee845d9b826218c69` | JPEG RGB; odd 3x5; RGB8; sRGB default |
| `png-exif-o6-2x3.png` | `565a26a2a68b97649d6e0cdcd6a8e52230c33cd8c557146a402b14f9ca78d963` | PNG eXIf orientation 6; top-left 3x2; first RGB pixel 55,195,127 |
| `png-gray-3x5.png` | `6e2d582cf4aee38f634a3edeb9bd1251b4f3230adf4c4f2cd945c3aec436b96f` | PNG gray; odd 3x5; R8 |
| `png-gray-alpha-5x3.png` | `096a8abecc525b690e233ff60e89ea7b45321e173ac532c07b5d77d0de6f1035` | PNG gray+straight-alpha; 5x3; RG8 |
| `png-gray16-3x2.png` | `028a0d9289cb13098dc4139767c04b10069f5513e0806699fd32f94cf80138db` | PNG 16-bit gray source normalized deterministically to R8 |
| `png-linear-4x2.png` | `443f20f1f8a1c0898b7b691ad1d1531c898635e8be1c83a0b8fc25e9459df428` | PNG gAMA 1.0; 4x2; linear RGB8 |
| `png-palette-3x3.png` | `4c0216f7759dd0ff87fe35a67170af0d0300fecc10dd2cc3ec1d01b5dba0bb60` | PNG palette expansion; odd 3x3; canonical RGB8 |
| `png-rgb-3x5.png` | `82beb5c6b47c2de8668eafa84220368d2fb5d99135e28bb5929f7d690b82fea7` | PNG RGB; odd/non-square 3x5; full chain 3x5,1x2,1x1 |
| `png-rgba-5x3.png` | `6e8619eba67f273fd328a275be6589d945db5da4cab761ec46bfddfac04a0a14` | PNG RGBA; odd 5x3; straight alpha retained |

JPEG pixel values are compared only against checked canonical decode evidence,
not against the lossless source pattern. PNG/HDR payloads, mip bytes, metadata,
digests, and normalized diagnostics are exact cross-platform evidence.

## Invalid Seeds And Mutation Matrix

| Seed | SHA-256 | Expected class |
|---|---|---|
| `hdr-bad-header.hdr` | `3eda465c0b6b694255be8f34e1bd1243558cb0ee891b7d49f89d949972b22197` | malformed or unsupported HDR header |
| `png-bad-crc.png` | `077281bb8b43c7f6dbe9c66fb4e529cf0fb9f486f2737dc12813035556f7a25a` | malformed PNG, inspect-stage `crc` evidence |
| `truncated-png.bin` | `30188ad79779aa1ccbd6d9c05106cbc6a42919a5b8c54e28d70eb7ca23388a6a` | truncated PNG |
| `unsupported.bin` | `2cfb61ab47d214b1469973e54edb14cee158fe16faa477c233e7c23b32dee253` | no matching importer |

The test matrix additionally creates prefixes of lengths 0 through 27 from
`png-rgba-5x3.png`. Together with the four immutable seeds this provides 32
bounded negative cases. Separate cases cover `NotFound`, `AccessDenied`,
missing/contradictory semantic settings, unknown enum values, source and
decoded-chain limits, registry conflict, importer ambiguity, and concurrent
extension lifetime behavior.

## Provenance

`generate_fixtures.py` documents the deterministic source patterns and
orientation metadata. It is a maintenance tool, not a test dependency. Any
regeneration change must update this manifest, its expected facts, and the
corresponding tests in the same commit.
