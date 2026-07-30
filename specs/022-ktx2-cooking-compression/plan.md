# Implementation Plan: KTX2 Cooking & Compression

**Branch**: `022-ktx2-cooking-compression` | **Date**: 2026-07-29 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/022-ktx2-cooking-compression/spec.md`

## Summary

Feature 022 turns validated Feature 021 textures into immutable KTX2 artifacts,
then realizes those artifacts as capability-compatible BC, ETC2/EAC, ASTC 4x4,
or uncompressed RHI textures. Asset owns CPU container, cook, inspection, load,
and transcode contracts while remaining `Asset -> Core`. Renderer owns ordered
target selection and request-scoped realization. RHI owns block-format
descriptions and usage capabilities. Vulkan owns native format queries, image
creation, upload, and evidence.

The implementation vendors KTX-Software 4.4.2 privately for standards-aligned
KTX2 parsing, uncompressed writing, validation, and Basis transcoding. Because
upstream documents native Basis encoding as non-bit-identical across host
architectures, the authoritative ETC1S/UASTC cook path executes one pinned,
single-threaded encoder WebAssembly module through a minimal vendored WAMR
2.4.5 interpreter. The module accepts canonical settings plus ordered raw mips
and returns the complete final compressed KTX2 byte stream. The host never
rewrites that compressed output; it preflights and reopens it with libktx.
Uncompressed KTX2 uses the host canonical writer. No timestamp, host path, SIMD
selection, thread scheduling, or backend order enters artifact bytes.

## Technical Context

**Language/Version**: C++20 with traditional public/private headers and sources; C for private WAMR integration; no C++20 Modules
**Primary Dependencies**: Existing Core, Asset, RHI, Renderer, Vulkan Backend; pinned private KTX-Software 4.4.2; pinned WAMR 2.4.5 interpreter; checked-in versioned encoder `.wasm`; SCons 4.10.1
**Storage**: Immutable in-memory KTX2 bytes and request-scoped transcode payloads; checked-in fixtures and golden digests; no manifest, derived-data cache, package, database, or cross-request cache
**Testing**: Existing `StonerTest` suites, KTX2 mutation corpus, mock RHI capability/upload tests, Asset architecture verifier, pinned `ktx validate` 4.4.2 CI oracle, strict Debug/Release, Linux ASan/UBSan and focused ThreadSanitizer, Windows/macOS/Linux GitHub Actions
**Target Platform**: Windows, macOS, Linux; CPU cook/inspect/load/transcode and mock realization mandatory on all three; native Vulkan evidence where the device advertises the required format usages
**Project Type**: Cross-platform C++ graphics-engine libraries and test executable
**Performance Goals**: Byte-identical 20-run cooks across three hosts; ETC1S <= 35% and UASTC <= 40% of RGBA8 mip bytes for the declared corpus; at least 35 dB/40 dB color PSNR; normal UASTC mean <= 3 degrees and p99 <= 10 degrees
**Constraints**: Asset depends only on Core; no RHI/native enums in artifacts; complete 2D mip chains only; synchronous request-scoped transcode; checked arithmetic before allocation; no platform variants, streaming, async manager, or cross-request cache
**Scale/Scope**: At least 18 valid artifacts, 40 malformed cases, 36 capability combinations, 8 concurrent requests; default maximum dimension 16,384, artifact 512 MiB, metadata 1 MiB, one level 512 MiB, aggregate target payload 1 GiB, and 15 levels

## Constitution Check

### Pre-Research Gate

- [x] **Spec-Driven Development**: `spec.md` defines four independently testable stories, 32 FRs, 13 measurable SCs, and seven accepted clarifications.
- [x] **Decoupled Architecture**: Asset remains `Asset -> Core`; Renderer bridges Asset to RHI; Vulkan alone owns native API details.
- [x] **Design Pattern Discipline**: preflight, container codec, cook policy, transcode, target selection, realization, and Vulkan mapping remain separate responsibilities.
- [x] **Multi-API Support**: compressed formats, block footprints, and capabilities are backend-neutral; Vulkan is the first implementation rather than the public contract.
- [x] **Advanced Graphics Readiness**: immutable KTX2 artifacts and block-aware layouts can later feed material/model assets, streaming, residency, ray tracing, and GI without changing identity.
- [x] **Naming Conventions**: public C++ design uses UE-style `F`, `E`, `I`, and `T` prefixes with PascalCase.
- [x] **Cross-Platform Compatibility**: the authoritative encoder uses identical checked-in WebAssembly bytecode and interpreter mode on Windows, macOS, and Linux.
- [x] **Automated Cross-Platform Validation**: all three hosts run focused CPU and mock-RHI tests plus strict Release; Linux retains sanitizer and available Vulkan native gates.

### Post-Design Gate

All gates remain satisfied. KTX-Software, WAMR, and the encoder module are
private implementation dependencies with exact versions, licenses, source
provenance, build recipe, and SHA-256 evidence. Public Asset headers expose none
of them. The added deterministic interpreter is justified by an explicit
upstream native-encoder portability limitation and remains isolated behind
`IKTX2Encoder`; no constitutional exception is required.

## Project Structure

### Documentation

```text
specs/022-ktx2-cooking-compression/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── ktx2-asset-api.md
│   ├── ktx2-container-profile.md
│   └── compressed-rhi-realization-api.md
└── tasks.md                         # Created later by speckit-tasks
```

### Planned Source Changes

```text
Source/
├── Asset/
│   ├── Public/Asset/
│   │   ├── EAssetResult.h
│   │   ├── FAssetDiagnostics.h
│   │   ├── FKTX2TextureArtifact.h
│   │   ├── FKTX2TextureCodec.h
│   │   ├── FTextureCook.h
│   │   ├── FTextureTranscode.h
│   │   ├── IAssetCooker.h
│   │   └── IAssetLoader.h
│   └── Private/
│       ├── FKTX2ContainerCodec.{h,cpp}
│       ├── FKTX2Preflight.{h,cpp}
│       ├── FKTX2TextureArtifact.cpp
│       ├── FKTX2TextureCodec.cpp
│       ├── FKTX2TextureCooker.cpp
│       ├── FKTX2TextureLoader.cpp
│       ├── FBasisTextureTranscoder.cpp
│       ├── FCanonicalBasisEncoder.cpp
│       ├── IKTX2Encoder.h
│       ├── FWamrEncoderRuntime.{h,cpp}
│       └── FTextureCookPolicy.cpp
├── RHI/Public/RHI/
│   ├── ERHIFormat.h
│   ├── FRHIDeviceCapabilities.h
│   ├── FRHIFormatInfo.h
│   ├── FRHITextureBufferCopyRegion.h
│   └── FRHITextureUploadDesc.h
├── Renderer/
│   ├── Public/Renderer/
│   │   ├── FKTX2TextureRealization.h
│   │   └── FTextureTargetProfile.h
│   └── Private/
│       ├── FKTX2TextureRealization.cpp
│       └── FTextureTargetSelection.cpp
└── Backend/Vulkan/
    ├── Public/VulkanRHI/FVulkanPhysicalDevice.h
    └── Private/
        ├── FVulkanDevice.cpp
        ├── FVulkanNativeContext.cpp
        ├── FVulkanPhysicalDevice.cpp
        └── FVulkanUploadStaging.cpp

ThirdParty/
├── ktx/
│   ├── LICENSE.md
│   ├── NOTICE
│   ├── UPSTREAM.md
│   ├── VERSION
│   └── <private libktx/Basis transcode subset>
├── stoner-basis-encoder/
│   ├── LICENSES/
│   ├── README.md
│   ├── SHA256SUMS
│   ├── VERSION
│   └── stoner_basis_encoder.wasm
└── wamr/
    ├── LICENSE
    ├── UPSTREAM.md
    ├── VERSION
    └── <minimal interpreter sources>

Tests/
├── AssetKTX2Tests.{h,cpp}
├── RendererKTX2TextureTests.{h,cpp}
├── RHICoreTests.cpp
├── VulkanBackendTests.cpp
├── VulkanNativeIntegrationTests.cpp
├── Fixtures/KTX2/{README.md,Valid/,Invalid/,Golden/}
├── verify_asset_layer.py
├── verify_ktx2_provenance.py
├── verify_ktx2_artifacts.py
└── test_verify_ktx2_artifacts.py
```

**Structure Decision**: Asset owns all CPU artifact and codec operations, but
third-party entry points remain in private adapters. RHI replaces implicit
bytes-per-texel assumptions with one block-aware format description. Renderer
adds a sibling KTX2 realizer instead of expanding the Feature 021 raw-texture
realizer into a mixed source/cooked manager. Vulkan extends its existing device,
upload, and readback paths through the shared RHI footprint helpers.

## Design Decisions

### 1. Deterministic Authoritative Encoder

KTX-Software 4.4.2 is pinned for libktx container operations and Basis
transcoding. Its native encoder is not authoritative because its release notes
document cross-platform byte differences. `FCanonicalBasisEncoder` instead runs
one checked-in WebAssembly module built from the pinned encoder sources through
WAMR 2.4.5 in interpreter mode. The module has a narrow memory-only ABI, no
WASI filesystem, clock, random, environment, network, SIMD, threads, or host
callbacks except bounded allocation and result copy.

The portable profile fixes encoder values and versions them:

- ETC1S Balanced: quality 192, compression level 2, one logical worker.
- ETC1S High: quality 255, compression level 2, one logical worker.
- UASTC Balanced: level 2, RDO disabled, one logical worker.
- UASTC High: level 3, RDO disabled, one logical worker.
- Normal maps set the encoder normal-map flag and preserve required XY/XYZ data.
- No zlib/zstd is applied after Basis encoding.
- Uncompressed LDR/HDR bypasses Basis and uses canonical libktx writing.

The first implementation gate compares the module on all three CI hosts over
the golden corpus before broader cooker work starts. This minimal digest job is
separate from the later full feature CI matrix. A mismatch is a blocking
failure; it must not be normalized away or resolved by weakening SC-002.

### 2. Typed Cook, Load, and Artifact Contracts

Add generic `FAssetCookParameters` and `FAssetLoadParameters` bases, optional
typed parameters on existing requests, and diagnostic lists on results. Existing
Feature 020 synthetic participants remain source-compatible through defaults.
`FTextureCookParameters` supplies the expected texture ID, immutable
`FTextureCookSettings`, and safety limits. The KTX2 cooker requires a validated
`FTextureAsset` payload and emits one `FKTX2TextureArtifact`.

`FKTX2TextureArtifact` is an immutable Asset payload containing canonical KTX2
bytes, normalized inspection data, source/content/cook evidence, and no decoded
or GPU cache. Different cook settings change `FAssetVersion`/digest, not
`FAssetId`. Loading preflights, opens, validates, and publishes exactly one
artifact or none.

### 3. Bounded Preflight Before Library Allocation

`FKTX2Preflight` is a small private structural reader, not a second KTX parser.
It reads only the fixed header, level index, and bounded metadata ranges with
checked `uint64` arithmetic. Before libktx receives image data it rejects wrong
scope, excessive dimensions/levels/metadata, out-of-range or overlapping
regions, invalid alignments, and impossible output budgets.

Default limits are dimension 16,384, artifact 512 MiB, metadata 1 MiB, 64 key
value entries, one stored level 512 MiB, and aggregate transcoded payload 1 GiB.
The complete level count is at most 15 at the default dimension. Callers may
raise positive limits but cannot disable checks. libktx remains authoritative
for DFD, Basis global data, payload integrity, and standards interpretation.

### 4. Canonical KTX2 Profile

Feature 022 writes 2D, one-face, non-array, depth-one artifacts with explicit
nonzero level count. Mip index zero is the logical base level. Required metadata
is inserted in lexicographic key order and contains no path or timestamp:

- `KTXorientation=rd`
- `KTXwriter=StonerGraphicsLab/022-v1`
- `stoner.assetId`
- `stoner.sourceDigest`
- `stoner.contentDigest`
- `stoner.cookRevision`
- `stoner.portableProfile`
- `stoner.semantic`
- `stoner.alphaMode`
- `stoner.mipPolicy`

DFD transfer is sRGB only for color sRGB; normal and data remain linear. Straight
alpha remains unassociated. Basis artifacts use undefined Vulkan format:
ETC1S carries the ETC1S DFD plus BasisLZ supercompression data, while UASTC
carries the UASTC DFD with no additional supercompression layer. HDR and
lossless data use explicit uncompressed KTX2 texel formats.
For ETC1S/UASTC, the WebAssembly module writes the complete final container and
`FKTX2ContainerCodec` only preflights, reopens, and compares normalized metadata
against the source contract. For uncompressed output,
`FKTX2ContainerCodec` is the canonical writer as well as the validator.

### 5. Asset-Owned Transcode Vocabulary

Asset exposes `ETextureTranscodeFormat`, `FTextureTranscodeRequest`, and an
immutable `FTranscodedTexturePayload`. These are codec/data terms, not RHI
types. Renderer maps one selected RHI format to the equivalent Asset transcode
target and independently checks the returned mip footprints.

Transcode output is transactional across all mips. Each level is produced into
temporary storage, checked against block count and exact bytes, then published
as one result only after every level succeeds. The payload exists only inside
one realization request. No static map, weak cache, registry record, or retained
handle is introduced.

### 6. Block-Aware RHI Format and Capability Model

Add `FRHIFormatInfo` with block width/height/depth, bytes per block, compressed,
sRGB, and depth/stencil facts. Checked helpers calculate block counts, tight row
bytes, slice bytes, and total bytes. `GetRHIFormatByteSize` becomes explicitly
uncompressed-only and returns zero for block-compressed formats; all texture
allocation, buffer-copy, upload, staging, and readback sizing migrates to the
format-info helpers.

RHI adds linear/sRGB color members for BC1, BC3, BC7, ETC2 RGB/RGBA, and ASTC
4x4 plus linear BC4, BC5, EAC R, and EAC RG. Capability data becomes
authoritative per-format records with `SampledImage` and `CopyDestination`
flags. `SupportsFormat` remains as a compatibility query over those records;
there is no second `SupportedFormats` source of truth.

Compressed upload regions must be block aligned at their origin. Width/height
must be block multiples unless that edge reaches the logical mip edge. Full
terminal mips smaller than 4x4 therefore consume exactly one full block.

### 7. Deterministic Renderer Selection and Rollback

`FTextureTargetProfile` is Renderer-owned and contains an explicit ordered RHI
format list plus an uncompressed-fallback flag. The desktop profile filters that
list by semantic, channel/alpha preservation, transfer, artifact codec, and
required RHI usage capability. Its default family order is BC, ASTC 4x4,
ETC2/EAC, then uncompressed:

- opaque color: BC1, BC7, ASTC, ETC2 RGB, RGBA8;
- alpha color: BC7, BC3, ASTC, ETC2 RGBA, RGBA8;
- two-channel normal/data: BC5, ASTC, EAC RG, RG8;
- one-channel data: BC4, EAC R, R8.

The requested transfer chooses the linear or sRGB member before capability
lookup. Profile order, not capability enumeration or enum value, is decisive.
The selected format name and rejected candidates are recorded in normalized
diagnostics.

`FKTX2TextureRealizer` validates, selects, transcodes or exposes uncompressed
levels, creates one `Sampled|CopyDestination` texture, uploads all mips, and
returns only after success. Creation, transcode, or any upload failure drops the
request-scoped payload and releases the RHI texture exactly once.

### 8. Vulkan Capability and Native Evidence

Vulkan maps every new RHI format to its `VkFormat`. Native capability collection
uses `vkGetPhysicalDeviceFormatProperties` and advertises sampled-image and
transfer-destination independently from optimal-tiling feature bits. Synthetic
fallback capabilities remain deterministic and explicit rather than pretending
to be native.

Native image creation uses the selected format. Tightly packed compressed
uploads use logical texel `imageExtent` and block-derived byte counts with zero
`bufferRowLength`/`bufferImageHeight`. Existing staging and readback code migrates
to common RHI footprint helpers. Native tests compare copied compressed block
bytes or a sampled result only when the device reports both required usages;
unavailable support is a named skip, not fallback success.

### 9. Validation and Independent Oracle

Check in at least 18 small golden artifacts and deterministic source inputs with
SHA-256 records. Generate at least 40 malformed cases by bounded mutation.
Focused tests cover policy, metadata, preflight, reopening, quality/size,
selection, transcode, block layout, rollback, leases, and eight-way concurrency.

`Tests/verify_ktx2_provenance.py` checks every vendored version, license, source
revision, and hash. `Tests/verify_ktx2_artifacts.py` invokes only pinned
`ktx validate 4.4.2` through an argument array and writes normalized JSON
evidence. CI installs or builds the pinned CLI separately from the engine
library, verifies its version/checksum, and runs
`ktx validate --format json --warnings-as-errors` on artifacts emitted by the
current test binary as well as checked-in golden files. The adapter accepts
diagnostic 7010 only for the exact complete, duplicate-free set of declared
`stoner.*` custom metadata keys; every other warning/error fails. Validator
absence may skip this supplementary local command but never the CI gate.

## Implementation Sequence

1. Vendor KTX-Software, WAMR, licenses/provenance, encoder module/build recipe, private warning policy, and architecture-verifier allowlist.
2. Add the independent validator adapter plus a minimal Windows/macOS/Linux deterministic encoder proof and golden digest gate before broader implementation.
3. Add typed generic cook/load parameters, stable result/stage extensions, KTX2 limits/settings/artifact/diagnostic public contracts, and content-version serialization.
4. Implement structural preflight, canonical metadata/profile writing, uncompressed KTX2, reopen/inspection, and bounded malformed tests.
5. Implement ETC1S/UASTC policy validation and the canonical WebAssembly encoder adapter; verify size, quality, metadata, and 20-run identity.
6. Implement KTX2 loader, Basis transcode targets, transactional per-mip publication, execution leases, and concurrent tests.
7. Replace RHI bytes-per-texel assumptions with `FRHIFormatInfo`, add compressed formats, usage capabilities, checked block footprints, and migrate all existing callers/tests.
8. Implement Renderer target profiles, deterministic selection, request-scoped KTX2 realization, fallback, diagnostics, and failure rollback.
9. Extend Vulkan mappings, queried format usage, staging/upload/readback footprints, fallback behavior, and conditional native evidence.
10. Complete valid/malformed/capability matrices, integrate the existing independent `ktx validate` adapter into the final workflow, add quality/size reports, sanitizer coverage, strict three-platform CI, and documentation reconciliation.

## Complexity Tracking

| Added Complexity | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Pinned WAMR interpreter plus encoder WebAssembly module | SC-002 requires byte-identical authoritative ETC1S/UASTC artifacts across arm64 and x86_64 hosts. | KTX-Software 4.4.2 explicitly documents native Basis output differences across platforms; thread count, `noSSE`, and disabled RDO do not provide the required guarantee. |
| Asset and RHI each have a compressed-format vocabulary | Asset must remain graphics-API/RHI independent while Renderer must select actual RHI formats. | Sharing `ERHIFormat` with Asset would violate the constitution; embedding codec choices only in strings would remove type safety and footprint validation. |
