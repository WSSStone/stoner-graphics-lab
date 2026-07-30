# Research: KTX2 Cooking & Compression

## KTX2 Library and Version

### Decision: Pin KTX-Software 4.4.2 as a private Asset dependency

Vendor the required libktx, Data Format Descriptor, Basis transcoder, and
license/notice sources from KTX-Software 4.4.2. Compile only memory-based KTX2
read/write/transcode support. Disable OpenGL/Vulkan loader helpers, source-image
decoders, command-line tools, zlib, and zstd in the engine library.

**Rationale**: KTX-Software is the Khronos reference library and 4.4.2 is the
current stable release. It aligns with KTX2 Specification Revision 4, includes
robustness fixes, and exposes the required `ktxTexture2_Create`,
`ktxTexture2_SetImageFromMemory`, `ktxTexture2_CompressBasisEx`,
`ktxTexture2_CreateFromMemory`, `ktxTexture2_TranscodeBasis`, and memory writer
APIs. Private vendoring fixes behavior and makes clean CI independent of system
package versions.

**Alternatives considered**:

- System libktx: rejected because runner/developer versions and build options
  would vary, undermining reproducibility and clean-machine setup.
- Handwritten full KTX2/Basis implementation: rejected because it duplicates a
  standards and malformed-input surface without adding engine value.
- Basis Universal alone: rejected because Feature 022 also needs standards-level
  KTX2 metadata, DFD, inspection, and independent compatibility with Khronos
  tools.

**Sources**: [KTX-Software 4.4.2 release](https://github.com/KhronosGroup/KTX-Software/releases/tag/v4.4.2),
[libktx API](https://github.khronos.org/KTX-Software/libktx/),
[KTX2 specification](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html).

## Cross-Platform Cook Determinism

### Decision: Use one pinned encoder WebAssembly module in a pinned WAMR interpreter

Authoritative ETC1S/UASTC cooking executes a checked-in, single-threaded,
memory-only WebAssembly module built from the pinned encoder source. Vendor the
minimal WAMR 2.4.5 interpreter and run only interpreter mode. The module exposes
a narrow ABI that consumes canonical settings plus tightly packed ordered raw
mips and returns one complete final ETC1S/UASTC KTX2 byte stream. It imports no
filesystem, clock, random, environment, network, SIMD, or threading service.
The host validates but never rewrites successful compressed output.

The module hash, source revision, WASI/Clang toolchain version, build command,
licenses, and exported ABI are recorded. CI rebuilds and hash-checks the module
in a dedicated provenance job when the required toolchain is available; normal
engine builds consume the checked-in module.

**Rationale**: KTX-Software 4.4.2 explicitly lists ETC1S and UASTC native
encoding as non-bit-identical across host platforms. It separately notes that
UASTC RDO can vary run-to-run unless threading is disabled. `threadCount=1`,
`noSSE=true`, and disabled RDO improve repeatability but do not resolve the
documented arm64/x86_64 difference. A single WebAssembly instruction stream is
the smallest local, offline mechanism that can satisfy the approved
cross-platform byte-identity requirement.

The encoder accepts only Feature 021 finite LDR UNorm inputs, so NaN payload
canonicalization is not part of its execution semantics. WAMR is built without
JIT/AOT, threads, WASI, sockets, or native function registration beyond bounded
memory copy.

**Alternatives considered**:

- Native libktx/Basis encoder with one thread and `noSSE`: rejected as the
  authoritative path because upstream still documents cross-platform output
  differences.
- Accept semantic/quality equality instead of byte equality: rejected because
  it contradicts FR-011 and SC-002.
- Docker or a remote canonical-cook service: rejected because local Windows and
  macOS development should remain offline and not require container
  virtualization or network infrastructure.
- Pre-cooked artifacts only: rejected because Feature 022 must provide a cooker,
  not only consume repository fixtures.
- External Wasmtime executable: rejected because the production cook path would
  depend on developer machine installation and process/filesystem behavior.

**Sources**: [KTX-Software 4.4.2 known issues](https://github.com/KhronosGroup/KTX-Software/releases/tag/v4.4.2),
[Basis Universal WASM/WASI support](https://github.com/BinomialLLC/basis_universal),
[WAMR embedding](https://github.com/bytecodealliance/wasm-micro-runtime),
[WAMR APIs](https://bytecodealliance.github.io/wamr.dev/apis/).

## Portable Compression Profiles

### Decision: Use versioned, fixed low-level settings and no post-Basis deflation

The initial portable profile ID is `stoner.ktx2.portable.v1`. It maps public
quality choices to immutable low-level encoder values:

| Policy | Initial profile | Low-level behavior |
|---|---|---|
| ETC1S | Balanced | quality 192, compression level 2, one worker, endpoint/selector RDO at library defaults fixed by producer version |
| ETC1S | High | quality 255, compression level 2, one worker, endpoint/selector RDO at library defaults fixed by producer version |
| UASTC | Balanced | UASTC level 2, RDO disabled, one worker |
| UASTC | High | UASTC level 3, RDO disabled, one worker |
| Uncompressed | Lossless | exact Feature 021 mip bytes in an explicit KTX2 texel format |

ETC1S is the default for ordinary LDR color. UASTC is the default for LDR
normal and optional higher-quality color. Generic Data defaults to uncompressed;
UASTC requires explicit lossy permission and ETC1S is forbidden. HDR is always
uncompressed float. The profile and producer version are cook-revision inputs.

**Rationale**: Fixed low-level settings make output reviewable and prevent
upstream default changes from silently changing artifacts. UASTC's raw 8 bpp is
25% of RGBA8 and already satisfies SC-004 without RDO or zstd. Omitting a second
deflation layer avoids another source of cross-architecture variability,
allocation, and startup cost.

**Alternatives considered**:

- Expose every Basis knob publicly: rejected because it creates an unstable API
  and an unbounded test matrix.
- Enable UASTC RDO by default: rejected because the feature's size target does
  not require it and upstream documents additional determinism constraints.
- Apply zstd to UASTC: rejected for the initial profile; Feature 025 may add a
  new producer/profile version after measuring package-level benefit.
- Use ETC1S for Data: rejected by clarification and semantic-loss policy.

**Source**: [ktxBasisParams reference](https://github.khronos.org/KTX-Software/libktx/structktxBasisParams.html).

## Canonical Container Profile

### Decision: Write explicit 2D scope, DFD transfer, fixed metadata, and nonzero levels

Artifacts have `pixelDepth=0`, `layerCount=0`, `faceCount=1`, and an explicit
nonzero level count. Level index entry zero describes the base mip even though
the physical mip payload area may be ordered for streaming constraints by the
KTX2 writer. Consumers use libktx's logical level APIs rather than manually
assuming file payload order.

Every artifact writes `KTXorientation=rd` for Feature 021 top-left origin and a
fixed `KTXwriter`. Required Stoner metadata records identity and digests but
never a native path, timestamp, host, registration order, address, or thread ID.
Keys are inserted in lexicographic order and values use a canonical UTF-8
representation. Duplicate required keys are rejected.

Both Basis models use `vkFormat=VK_FORMAT_UNDEFINED`. ETC1S uses its required
DFD and BasisLZ supercompression global data. UASTC uses its required DFD with
no additional supercompression layer in the v1 profile. Uncompressed output
uses the explicit Vulkan format mandated by KTX2 as a container interchange
token; that value remains inside the private KTX codec and is not exposed in
Asset metadata or public headers.

**Rationale**: Explicit orientation and transfer remove API convention
ambiguity. Fixed metadata permits byte-level version evidence. Treating libktx
as the logical-level authority avoids confusing level-index order with physical
streaming order.

**Alternatives considered**:

- Omit orientation because `rd` may be assumed: rejected because FR-007 requires
  inspectable preservation and contradictions must be diagnosable.
- Store JSON metadata: rejected because canonicalization and parser complexity
  are unnecessary for a small fixed field set.
- Embed RHI or Vulkan target preferences: rejected because the authoritative
  artifact is platform independent and Asset must remain backend neutral.

**Source**: [KTX2 file structure, level index, and orientation](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html).

## Bounded Inspection and Validation

### Decision: Perform a structural preflight before libktx image-data allocation

Use a small private `FKTX2Preflight` to inspect the 80-byte header, checked level
index, and bounded DFD/KVD/SGD ranges. It verifies source bounds, alignment,
overlap, supported 2D scope, dimensions, level count, and configured budgets
before opening with `KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT`.

libktx then validates DFD, supercompression global data, metadata, and payload.
The engine compares normalized container facts with the artifact's Stoner
metadata and source texture contract. Parsing and load publish nothing until all
checks pass.

**Rationale**: A mature parser is still allowed to allocate based on input
claims. A narrow checked preflight enforces project limits before that point
without becoming a competing full KTX implementation.

**Alternatives considered**:

- Trust repository-cooked files: rejected because future packages and local
  files are untrusted input.
- Fully parse KTX2 ourselves before libktx: rejected as duplicated standards
  logic.
- Catch allocation failure after open: rejected because FR-013 requires
  excessive inputs to fail before excessive allocation.

## Runtime Transcoding

### Decision: Keep codec output in Asset vocabulary and publish all mips atomically

Asset defines a typed transcode-target enum for BC1/3/4/5/7, ETC2 RGB/RGBA, EAC
R/RG, ASTC 4x4, and uncompressed R/RG/RGBA fallback. Renderer maps selected RHI
formats to that enum. libktx/Basis transcodes complete levels into temporary
storage. Each level's block count and exact byte size are checked before the
immutable aggregate is returned.

Normal/Data plus sRGB, alpha-dropping, and required-channel-dropping targets fail
before transcode. sRGB is not combined with BC4/BC5/EAC R/EAC RG. Output is held
only by the current realization stack and is not registered or cached.

**Rationale**: Asset needs codec vocabulary to produce CPU bytes but cannot
depend on RHI. Atomic aggregate publication prevents a valid prefix of mips from
escaping after a later failure. Request ownership directly implements the
clarified no-cache lifetime.

**Alternatives considered**:

- Return one mip at a time to Renderer: rejected because failure could expose a
  partial texture payload and complicate rollback.
- Put target selection in Asset: rejected because device capabilities belong to
  RHI/Renderer.
- Cache transcodes in the artifact: deferred to Feature 026, which owns request
  coalescing and retained lifetime.

**Sources**: [libktx transcoding API](https://github.khronos.org/KTX-Software/libktx/group__transcoding.html),
[Basis Universal supported targets](https://github.com/BinomialLLC/basis_universal).

## RHI Block Layout

### Decision: Model every format by block extent and bytes per block

Introduce one `FRHIFormatInfo` table. Uncompressed formats are 1x1x1 blocks;
Feature 022 compressed formats use 4x4x1 blocks. BC1, BC4, ETC2 RGB, and EAC R
use 8 bytes per block. BC3, BC5, BC7, ETC2 RGBA, EAC RG, and ASTC 4x4 use 16
bytes per block.

Required bytes are:

`ceil(width / blockWidth) * ceil(height / blockHeight) *
ceil(depth / blockDepth) * bytesPerBlock`, with checked multiplication and row
pitch rules. A 1x1 terminal compressed mip therefore stores one block. Partial
compressed upload offsets are block aligned; an extent may end off-block only
when it reaches the logical mip edge.

**Rationale**: Existing bytes-per-texel helpers cannot represent compressed
formats and would reject valid terminal mips or underallocate staging buffers.
The same table must drive descriptors, upload validation, copies, allocation
estimates, staging, readback, mocks, and native backends.

**Alternatives considered**:

- Special-case compressed uploads in Vulkan: rejected because mocks and later
  Metal/DX12 need the same public contract.
- Treat bytes per block as bytes per texel: rejected because it produces
  incorrect row and total sizes.
- Require dimensions divisible by four: rejected because native APIs and KTX2
  support smaller edge blocks and complete mip chains naturally end below 4x4.

## Capability and Target Selection

### Decision: Use per-format usage flags and profile-ordered filtering

Replace the authoritative plain `SupportedFormats` list with sorted
`FRHIFormatCapabilities` records. At minimum they carry `SampledImage` and
`CopyDestination`. Renderer traverses the explicit target profile in order and
selects the first member that preserves semantic, channels, alpha, transfer,
and required usage.

The desktop family order is BC, ASTC 4x4, ETC2/EAC, then uncompressed. Format
order is specialized by semantic and alpha so BC1 cannot win for alpha-bearing
color and BC4 cannot win for two-channel data. Capability array order and enum
numeric value are irrelevant.

**Rationale**: A format may exist but not support the usage needed for texture
realization. Explicit profile order gives deterministic policy while leaving
future mobile/web profiles free to prefer ASTC or ETC.

**Alternatives considered**:

- One boolean per compression family: rejected because support differs by exact
  format, transfer variant, and usage.
- Choose the first advertised device format: rejected because driver
  enumeration order is not a product policy.
- Put target order in KTX2 metadata: rejected because runtime preferences must
  not recook or version the portable artifact.

## Vulkan Mapping and Upload

### Decision: Query optimal-tiling sampled and transfer-destination support per VkFormat

Map every required RHI member centrally. During native adapter discovery,
`vkGetPhysicalDeviceFormatProperties` determines whether optimal-tiling features
support sampled images and transfer destination. Advertise each usage
independently. Image creation and upload use the selected `VkFormat`; tightly
packed `VkBufferImageCopy` regions use logical texel extents and RHI-computed
block byte lengths.

**Rationale**: Vulkan format existence does not imply every image usage. The
query and actual create/upload behavior must agree with Renderer selection.
Logical image extent and memory block footprint are distinct native concepts.

**Alternatives considered**:

- Hard-code desktop support: rejected because macOS MoltenVK, software Vulkan,
  and physical devices differ.
- Probe by attempting image creation only: rejected because it is expensive,
  loses per-usage diagnostics, and still needs deterministic selection input.
- Expose `VkFormatProperties` through RHI: rejected as a backend leak.

**Sources**: [Vulkan format properties](https://registry.khronos.org/vulkan/specs/latest/man/html/vkGetPhysicalDeviceFormatProperties.html),
[VkBufferImageCopy](https://registry.khronos.org/vulkan/specs/latest/man/html/VkBufferImageCopy.html).

## Independent Validator and Test Corpus

### Decision: Run pinned `ktx validate` 4.4.2 as a CI oracle

The engine first writes and reopens each artifact. A separate Python adapter then
invokes `ktx validate --format json --warnings-as-errors` with no shell and
normalizes the report. CI verifies tool version/checksum. Khronos diagnostic
7010 is accepted only for the exact complete set of nine declared
`stoner.*` custom metadata keys; any missing, duplicate, additional, or other
warning/error fails. This exception is necessary because custom KTX2 keys are
legal but the pinned validator reports every custom key as a warning, which
`--warnings-as-errors` promotes to a nonzero exit. Invalid fixtures remain
engine mutation tests; they are not required to share exact diagnostic
categories with the external tool.

At least 18 valid artifacts cover policy, semantics, alpha, odd dimensions,
small mips, and HDR. At least 40 bounded mutations cover structural and payload
failures. Quality uses decoded/transcoded reference comparisons; size excludes
fixed container metadata as SC-004 specifies.

**Rationale**: Reopening with the same linked library cannot independently prove
that the resulting bytes satisfy the specification. The Khronos CLI applies
specification-based validation and has machine-readable JSON plus stable exit
codes.

**Alternatives considered**:

- Use only engine inspection: rejected by FR-030.
- Require developers to install any `ktx` version: rejected because evidence
  would drift.
- Link the CLI into tests: rejected because it would no longer be an independent
  executable workflow.

**Source**: [`ktx validate` reference](https://github.khronos.org/KTX-Software/ktxtools/ktx_validate.html).
