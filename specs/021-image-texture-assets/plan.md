# Implementation Plan: Image & Texture Asset Foundation

**Branch**: 021-image-texture-assets | **Date**: 2026-07-29 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from specs/021-image-texture-assets/spec.md

## Summary

Feature 021 introduces the first concrete Asset payloads. PNG, JPEG, and Radiance
HDR sources become validated immutable FImageAsset and FTextureAsset outputs.
Asset remains CPU-only and depends only on Core. It uses Feature 020 identity,
registry, importer dispatch, lease, version, and diagnostics contracts.

A Renderer adapter, rather than Asset, maps a texture payload to a sampled RHI
texture and uploads every mip through one small backend-neutral RHI upload
contract. Asset preserves RGB canonical payloads; Renderer expands them to
portable RGBA RHI formats where a backend cannot sample RGB directly.

A pinned Asset-private stb_image 2.30 decoder handles only PNG, JPEG, and
Radiance HDR. Project code performs source limits, container inspection,
transfer/orientation policy, canonicalization, semantic mips, and stable
diagnostics around that decoder.

## Technical Context

**Language/Version**: C++20, traditional public/private headers and sources; no Modules
**Primary Dependencies**: Existing Core, Asset, RHI, Renderer; C++ standard library; pinned private stb_image 2.30; SCons 4.10.1
**Storage**: Process-local immutable CPU payloads and checked-in fixtures; no database, manifest, cooked cache, or GPU handle in Asset
**Testing**: Existing StonerTest Asset suite, mock-RHI realization tests, Asset boundary verifier, strict Debug/Release, Linux ASan/UBSan plus focused ThreadSanitizer concurrency coverage, GitHub Actions Windows/macOS/Linux
**Target Platform**: Windows, macOS, Linux; headless decode and mock-RHI tests mandatory; native Vulkan upload/readback supplementary
**Project Type**: Cross-platform C++ graphics-engine libraries and test executable
**Performance Goals**: Identical output over 20 repeats; reject over-limit input before decoded-payload allocation; support configured limits without integer overflow
**Constraints**: Asset -> Core only; public Asset headers expose neither stb nor RHI; Renderer is the only Asset-to-RHI bridge; no KTX2, compression, streaming, cube/array/volume, or async manager
**Scale/Scope**: PNG/JPEG/Radiance HDR; default source <= 256 MiB, one mip <= 512 MiB, full chain <= 1 GiB, dimension <= 16,384; one request emits Image plus dependent Texture

## Constitution Check

### Pre-Research Gate

- [x] **Spec-Driven Development**: spec.md has four independently testable stories, 28 FRs, 12 measurable SCs, and five accepted clarifications.
- [x] **Decoupled Architecture**: Asset remains Asset -> Core; Renderer depends on Asset + RHI + Core; Vulkan remains the only native API owner.
- [x] **Design Pattern Discipline**: inspection, decoder strategy, semantic mip generation, publication, and realization are separate services; IAssetImporter remains the format Strategy.
- [x] **Multi-API Support**: Asset formats are API-neutral; Renderer maps to RHI; each backend realizes/uploads independently.
- [x] **Advanced Graphics Readiness**: explicit semantic/color/mip/version data feed later material, KTX2, mesh, streaming, ray tracing, and GI phases.
- [x] **Naming Conventions**: public names use UE-style F/E/I/T prefixes and PascalCase.
- [x] **Cross-Platform Compatibility**: source access, scalar mip math, checked layout, and fixtures are platform-independent.
- [x] **Automated Cross-Platform Validation**: existing Actions jobs retain --suite asset, full regression, strict Release, and Linux ASan/UBSan; Feature 021 adds Asset coverage plus a focused Linux ThreadSanitizer concurrency gate.

### Post-Design Gate

All gates remain satisfied. The only third-party code is pinned private decoder
implementation with checked-in license, version, and hash. The verifier rejects
third-party use in Asset public headers. No constitutional exception is needed.

## Project Structure

### Documentation

~~~
specs/021-image-texture-assets/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── image-texture-asset-api.md
└── tasks.md                         # Created later by speckit-tasks
~~~

### Planned Source Changes

~~~
Source/
├── Asset/
│   ├── Public/Asset/
│   │   ├── FImageAsset.h
│   │   ├── FImageImport.h
│   │   ├── FImageInspection.h
│   │   ├── FImageMip.h
│   │   ├── FImageTypes.h
│   │   └── FTextureAsset.h
│   └── Private/
│       ├── FImageAsset.cpp
│       ├── FImageContainerInspector.{h,cpp}
│       ├── FImageDecode.{h,cpp}
│       ├── FImageImport.cpp
│       ├── FImageMipGenerator.{h,cpp}
│       ├── FImageOrientation.{h,cpp}
│       ├── FImageValidation.{h,cpp}
│       └── FStbImageDecode.cpp
├── RHI/Public/RHI/
│   ├── ERHIFormat.h
│   ├── FRHITextureUploadDesc.h
│   └── IRHIDevice.h
├── Renderer/
│   ├── Public/Renderer/FTextureAssetRealization.h
│   └── Private/FTextureAssetRealization.cpp
└── Backend/Vulkan/
    ├── Public/VulkanRHI/FVulkanDevice.h
    └── Private/{FVulkanDevice,FVulkanNativeContext,FVulkanPhysicalDevice}.cpp

ThirdParty/stb/{LICENSE.md,VERSION,stb_image.h}

Tests/
├── AssetImageTextureTests.{h,cpp}
├── AssetTests.{h,cpp}
├── RendererTextureAssetTests.{h,cpp}
├── Fixtures/Images/{README.md,Valid/,Invalid/}
└── verify_asset_layer.py
~~~

**Structure Decision**: Asset owns decode, validation, mips, and import/registry
publication. RHI owns portable upload vocabulary. Renderer owns mapping,
conversion, and rollback. Vulkan owns native staging/copy/transition. An Asset
test aggregate retains the existing --suite asset CLI contract while keeping
Feature 020 and 021 test files focused.

## Design Decisions

### 1. Backward-Compatible Import Request and Atomic Publication

Feature 020 importers receive only descriptor and source, so they cannot carry
typed settings or requested output identities. Add FAssetImportRequest with
descriptor, source lease, and immutable typed FAssetImportParameters. Keep the
old importer overload as a default bridge for Feature 020 synthetic importers.

FImageImportParameters contains ImageId, TextureId, semantic, color space, HDR
layout, mip policy, and limits. FAssetImportService::ImportAndRegister selects
the importer through FAssetDispatch, validates all outputs, applies one
FAssetMutationBatch, and returns either both Image/Texture payloads or none.
Feature 026, not this feature, owns payload caching and asynchronous loading.

### 2. Decoder and Metadata Boundary

Vendor exact upstream stb_image 2.30 under ThirdParty/stb with license, version,
upstream commit, and SHA-256 provenance. Compile exactly one Asset-private
translation unit with STB_IMAGE_IMPLEMENTATION, STBI_NO_STDIO, STBI_NO_SIMD,
STBI_ONLY_PNG, STBI_ONLY_JPEG, and STBI_ONLY_HDR. Decoder input comes only from
FAssetSourceLease; no native file path reaches stb.

FImageContainerInspector precedes decode. It identifies signatures, validates
source/dimension limits, validates PNG chunk structure/CRC, scans JPEG markers,
and reads transfer/profile/orientation metadata. It accepts PNG sRGB or exact
representable gAMA, defaults untagged PNG/JPEG color to sRGB, defaults Radiance
HDR to Linear, and rejects ICC/nonrepresentable profiles. It extracts JPEG APP1
and PNG eXIf orientation, rejects CgBI/premultiplied PNG, and normalizes output
to DX/Unreal top-left origin.

### 3. Canonical Payloads and Version Evidence

LDR output is R8, RG8, RGB8, or RGBA8. Palette PNG expands before conversion;
supported 1/2/4/8/16-bit PNG normalizes deterministically to 8-bit UNorm; JPEG
keeps decoded gray or RGB channels. RGB stays RGB in Asset.

Radiance HDR decodes as linear float. Default texture layout is RGBA16F;
RGBA32F and no-alpha RGB32F require explicit settings. Finite values outside
the selected float range fail with HDRPrecisionRangeExceeded rather than clamp.
The source digest covers exact bytes; Image content digest covers canonical base
data and interpretation; Texture digest additionally covers settings and every
mip byte.

### 4. Deterministic Semantic Mips

Full-chain generation is default; BaseOnly is explicit and versioned. Each level
uses max(1, floor(previous / 2)) and a scalar fixed-order full-footprint box
resampler. No SIMD, native resize, random seed, or host color-management service
is on the output path.

- sRGB RGB uses checked-in literal transfer lookup tables, fixed-point
  linear-light accumulation, and a literal inverse table; straight alpha is
  averaged independently.
- Linear UNorm data uses integer weighted accumulation. Float HDR uses
  fixed-order scalar IEEE-754 addition/division only, with no reassociation or
  fused multiply-add opportunity; output rounds once to its declared layout.
- Normal accepts RG8, RGB8, and RGBA8. RG reconstructs +Z; RGB/RGBA use RGB.
  Filtered vectors normalize deterministically; a zero result becomes (0, 0, 1).
- Data filters channels independently with no color or vector operation.

The complete planned chain validates before allocation and never mutates the
source Image payload.

CPU metadata, canonical payload bytes, mip bytes, version evidence, and
diagnostics compare exactly. Decoded stored normal samples satisfy
`abs(length - 1.0) <= 0.015`; native upload/readback is supplementary and uses
one-LSB UNorm, `max(1e-3, abs(expected) * 1e-3)` FP16, and
`max(1e-6, abs(expected) * 1e-6)` FP32 per-channel tolerances. This separates
the deterministic CPU contract from representational native smoke evidence.

### 5. Renderer-to-RHI Realization

Asset owns EImageTexelFormat and never includes ERHIFormat. RHI gains only the
portable formats needed here: R8G8_UNorm, R8G8B8A8_sRGB, and
R32G32B32A32_Float, alongside existing R8, RGBA8, RGBA16F, and float formats.
FRHITextureUploadDesc plus synchronous IRHIDevice::UploadTexture validates one
mip and returns success only when it is sample-ready. This is intentionally not
an asynchronous streaming API.

FTextureAssetRealizer belongs to Renderer. It creates one 2D
Sampled|CopyDestination texture, uploads mips in ascending order, and returns it
only after all uploads succeed. RGB8 linear expands to RGBA8_UNorm; RGB8 sRGB to
RGBA8_sRGB; RGB32F to RGBA32F; alpha becomes one. R8/RG8 color uses grayscale
semantics and expands to RGBA, with RG8 supplying straight alpha. R8/RG8 linear
data/normal remain native.

A create/upload failure releases all request-owned GPU state and reports
stage/mip. Asset CPU payload remains valid. Vulkan implements generic upload by
staging, CopyDestination -> ShaderReadOnly transition, and completion wait. The
deterministic fallback records the same validated upload footprint.

### 6. Diagnostics, Limits, and Boundary Enforcement

Extend EAssetResult/EAssetStage only where existing generic values cannot
distinguish required cases: TruncatedSource, ImageLimitExceeded,
UnsupportedColorProfile, NonFiniteImageData, HDRPrecisionRangeExceeded; plus
Decode, Validate, and Mip stages. Renderer owns a separate realization
diagnostic around ERHIResult.

FImageImportLimits defaults to 16,384 pixels per dimension, 256 MiB source,
512 MiB per mip, and 1 GiB full chain. Callers may raise a bound but cannot
disable checks. Every size calculation is checked uint64 before read, allocation,
row pitch, or RHI upload.

Update Tests/verify_asset_layer.py to allow only
FStbImageDecode.cpp -> ThirdParty/stb/stb_image.h. Public Asset headers and all
other Asset paths remain third-party-free; Asset SConscript keeps Core as its
only engine-layer dependency.

## Implementation Sequence

1. Add decoder provenance, scoped third-party warning handling, and verifier update.
2. Add import-request bridge, public Image/Texture types, traits, diagnostics, inspection, limits, and metadata/version attributes.
3. Implement bounded read, signature/container inspection, transfer/orientation parsing, error categories, and decoder wrapper.
4. Implement canonical LDR/HDR conversion, top-left transforms, immutable validation, and digest inputs.
5. Implement semantic mip generation, Texture validation, and atomic ImportAndRegister.
6. Implement PNG/JPEG/HDR importer registration plus dispatch/lifetime/transaction tests.
7. Extend RHI formats/capabilities and synchronous texture upload in mocks and Vulkan.
8. Implement Renderer mapping, RGB expansion, rollback, and diagnostics.
9. Add the minimum fixture/mutation matrix, focused/unit/integration/architecture tests, warning cleanup, ASan/UBSan and focused ThreadSanitizer coverage, and CI wording/artifacts.
10. Run quickstart gates and post-implementation documentation hook.

## Complexity Tracking

No constitution violation requires a complexity exception.
