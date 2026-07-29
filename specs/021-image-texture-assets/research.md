# Research: Image & Texture Asset Foundation

## Decoder Strategy

### Decision: Vendor stb_image 2.30 privately and compile only PNG, JPEG, and Radiance HDR

Use exact upstream stb_image.h v2.30 in ThirdParty/stb with its license, version,
upstream commit, and SHA-256 recorded. One private Asset translation unit defines
STB_IMAGE_IMPLEMENTATION, STBI_NO_STDIO, STBI_NO_SIMD, STBI_ONLY_PNG,
STBI_ONLY_JPEG, and STBI_ONLY_HDR.

**Rationale**: The upstream header supports the three required formats, memory
or callback input, and per-build decoder selection without a runtime dependency.
STBI_NO_STDIO makes source access flow through IAssetSource. STBI_NO_SIMD keeps
architecture-specific codec execution out of the byte-determinism boundary.

**Alternatives considered**:

- Platform image APIs: rejected because Windows, macOS, and Linux would differ
  in decode, metadata, and color-management behavior.
- libpng plus libjpeg-turbo plus a Radiance decoder: rejected because three
  independent build and security-update surfaces are excessive before Asset
  contracts are established.
- Handwritten decoders: rejected because mature codec reimplementation adds
  input safety risk without advancing engine architecture.

**Sources**: [stb_image v2.30](https://github.com/nothings/stb/blob/master/stb_image.h),
[stb integration guidance](https://github.com/nothings/stb).

## Container Inspection and Metadata

### Decision: Inspect before decode and accept only metadata representable by the contract

FImageContainerInspector owns signature detection, bounded header parsing, PNG
chunk/CRC validation, JPEG marker scanning, transfer/profile policy, and EXIF
orientation extraction. It accepts explicit sRGB and exact representable gamma,
defaults untagged PNG/JPEG color to sRGB and Radiance HDR to Linear, and rejects
ICC/nonrepresentable profile data. It applies JPEG APP1 and PNG eXIf orientation
so output is always DX/Unreal top-left.

**Rationale**: stb_image is a pixel decoder, not a complete stable metadata and
color-management policy. Owning this small policy prevents decoder upgrades or
host facilities from changing Asset bytes, identity evidence, or V direction.

**Alternatives considered**:

- Ignore metadata: rejected because it breaks explicit transfer and origin FRs.
- Full ICC transforms: deferred because color management is a separate
  dependency/testing domain; Feature 021 supports only Linear/sRGB.
- Preserve per-source origin in metadata: rejected because later material/model
  users would need format-specific flip behavior.

## CPU Format and HDR Precision

### Decision: Use the clarified finite canonical set and preserve RGB in Asset

LDR formats are R8, RG8, RGB8, and RGBA8. RG8 represents grayscale plus straight
alpha when its semantic is Color. HDR formats are RGB32F, RGBA16F, and RGBA32F.
HDR defaults to RGBA16F; RGBA32F and RGB32F are explicit. Values outside
the selected float layout fail rather than clamp. PNG 16-bit source normalizes
deterministically to 8-bit because the feature does not define 16-bit UNorm
Asset payloads.

**Rationale**: A small set controls validation/mip/RHI combinations, preserves
compact normal/data formats, and is a stable KTX2 cooking input. It implements
all clarification decisions without making source storage a runtime contract.

**Alternatives considered**:

- Preserve every source channel count and precision: rejected because the
  payload, mip, and RHI matrix becomes too broad.
- Convert all data to RGBA32F: rejected because common LDR textures become
  unnecessarily large.
- Default HDR to RGBA32F: rejected by clarification; high precision is explicit.

## Cross-API RGB Realization

### Decision: Preserve RGB CPU assets, expand only in Renderer to portable RGBA RHI formats

Renderer expands RGB8 to RGBA8 UNorm/sRGB and RGB32F to RGBA32F while filling
alpha with one. RHI does not promise an RGB8 sampled format. It adds only RG8
UNorm, RGBA8 sRGB, and RGBA32F, plus mapping/capability coverage.

**Rationale**: Vulkan defines VK_FORMAT_R8G8B8_UNORM, but a general RGB8 sampled
format is not portable across planned APIs. The DXGI catalog has no equivalent
general R8G8B8 UNorm sampled format; its packed RGB entries are YUV-like
two-pixel encodings. Keeping conversion in Renderer preserves Asset semantics
and avoids a future backend portability leak.

**Alternatives considered**:

- Expand RGB during Asset import: rejected because clarification requires RGB
  preservation and it inflates CPU/cooked input.
- Require every backend to emulate RGB8: rejected because it creates an
  accidental RHI promise.
- Reject RGB source: rejected because JPEG and ordinary PNG naturally use it.

**Sources**: [Vulkan VkFormat](https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkFormat.html),
[DXGI formats](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format).

## Deterministic Mip Generation

### Decision: Full-chain default, fixed-order scalar semantic resampling

Use a full-footprint box resampler with fixed iteration order. sRGB LDR uses
checked-in literal transfer tables and fixed-point linear-light accumulation.
Linear UNorm uses integer accumulation. Float HDR uses fixed-order scalar
IEEE-754 addition/division and one final rounding to its layout. Normal maps decode to
vectors, filter/normalize, and use +Z for zero vectors. Alpha stays straight and
filters independently.

Canonical CPU metadata, payloads, mips, version evidence, and diagnostics use
exact comparisons. Decoded stored normal samples use an absolute unit-length
tolerance of 0.015. Native upload/readback is supplementary: UNorm channels
use one LSB (`1 / 255`), FP16 channels use
`max(1e-3, abs(expected) * 1e-3)`, and FP32 channels use
`max(1e-6, abs(expected) * 1e-6)`.

**Rationale**: The feature requires byte-identical output across toolchains, not
only visually similar images. Generic resize libraries, SIMD, native color
services, or GPU mip generation introduce uncontrolled variability and do not
encode color/normal/data semantics.

**Alternatives considered**:

- GPU mip generation: rejected because Asset must run without graphics and later
  cooking needs CPU data.
- stb resize helper: rejected because it is another output dependency and has no
  project semantic policy.
- Floating gamma formulas: rejected because final-byte math can differ by
  platform; literal tables are reviewable and stable.

## RHI Upload Boundary

### Decision: Add synchronous IRHIDevice::UploadTexture with one mip descriptor

RHI gains FRHITextureUploadDesc and an IRHIDevice::UploadTexture call. The
device owns staging, queues, transitions, and completion. Renderer creates a
Sampled|CopyDestination texture and uploads mips in order. Success means the
mip is sample-ready. No asynchronous request handle is introduced.

**Rationale**: Existing upload staging is Vulkan-specific and cannot be used by
Renderer without breaking the constitution. Device-level ownership matches native
queue/submission lifetime and permits deterministic mocks plus future Metal/DX12
implementations.

**Alternatives considered**:

- Cast to FVulkanDevice from Renderer: rejected by the RHI/Backend boundary.
- Place UploadTexture on IRHITexture: rejected because a texture does not own
  queue/submission lifetime.
- Add async upload now: deferred to Feature 026 asset request/cancellation
  lifecycle.

## Fixture and Validation Strategy

### Decision: Check in a minimal valid corpus and produce most negative cases by mutation

Retain at least 12 small binary PNG/JPEG/HDR fixtures with recorded source
SHA-256 and decoded expectations. Generate at least 30 truncated, corrupted,
checksum, limit, metadata-conflict, non-finite, missing, unsupported, and
excessive cases by bounded in-memory mutations. Extend the existing Asset
suite, Asset architecture verifier, all three CI jobs, Linux ASan/UBSan, and a
focused Linux ThreadSanitizer gate with at least eight concurrent requests.

**Rationale**: Checked-in fixtures make decode behavior reviewable; mutations
give broad exact failure coverage without a large opaque binary corpus. Feature
020 already established the focused Asset suite as the three-platform CI path.

**Alternatives considered**:

- System image corpus: rejected because runners and developer machines differ.
- Runtime fixture generation by a second codec: rejected because it hides or
  duplicates codec assumptions.
- Visible screenshot requirement: rejected because this is CPU asset ingestion;
  mock RHI and supplementary native upload/readback are sufficient.
