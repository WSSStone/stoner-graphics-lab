# Feature 029 Output-Transform Authorities

This file freezes the normative upstream identities used to create the Feature
029 CPU oracle, shaders, and canonical vectors. Upstream repositories are
reference inputs only; Feature 029 has no runtime dependency on them. A change
to any identity, equation, constant, matrix, or boundary policy requires a new
transform/profile/vector-set identity and an affected workload revision.

## Khronos PBR Neutral

- Repository: `https://github.com/KhronosGroup/ToneMapping.git`
- Commit: `b5a2eed5ddf6c2227090449399de9c7affb9e4c9`
- File: `PBR_Neutral/pbrNeutral.glsl`
- SHA-256: `087a1542c8d12d2d0fb89f570057e4e8365cc04eee5bff76c6e365cfa69f3734`
- License declared upstream: Apache-2.0 for code; the adjacent specification
  text is CC-BY-4.0.
- Feature identity: `Sdr.KhronosPbrNeutral.v1`

The frozen implementation uses `F90=0.04`, start compression `0.8-F90=0.76`,
and desaturation `0.15`, with non-negative linear Rec.709 input and linear
Rec.709 output.

## Narkowicz Sampled ACES Fit

- Authority: `https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/`
- Feature identity: `Sdr.NarkowiczAcesFit.v1`
- Frozen equation:
  `clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0,1)`

This is explicitly a sampled fit to an ACES RRT+ODT response. It is not an
Academy ACES Output Transform and must never be labelled as ACES conformance.

## Extended Reinhard

- Paper: Erik Reinhard et al., *Photographic Tone Reproduction for Digital
  Images*, `https://www-old.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf`
- Feature identity: `Sdr.ExtendedReinhardRec709.v1`
- Frozen Rec.709 luminance coefficients: `[0.2126, 0.7152, 0.0722]`
- Frozen white luminance: `Lwhite=4.0`
- Frozen equation: `Yd=Y*(1+Y/(Lwhite^2))/(1+Y)`, followed by zero-safe
  luminance-preserving RGB scaling.

`Lwhite` is configuration authority and is never derived from a frame.

## Official ACES 2 HDR Viewing Transform

- Parent repository: `https://github.com/aces-aswf/aces.git`
- Tag: `v2.0.0+2025.04.04`
- Tag target: `35e1e6ac2c26ec75433547d5d0a3a881f39bd9f5`
- Feature identity: `Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1`

Frozen parent submodules:

| Component | Commit |
|---|---|
| `aces-amf` | `78e8d8175f6408c1eba5575e54a69b4cf363073b` |
| `aces-core` | `2d7af39344725aaa8ac3bf1746693c9a1d6c4792` |
| `aces-input-and-colorspaces` | `cc60becf11534f2a40df00a281f47e95412d0599` |
| `aces-look` | `e0e70ecf2ce57d503318dde1fa4aa1bd7287fcdd` |
| `aces-output` | `aab74723f76728c37345ed01e51ebb24fb1f2f1f` |

Normative files used by the bounded forward implementation:

| Component-relative file | SHA-256 |
|---|---|
| `aces-core/lib/Lib.Academy.Utilities.ctl` | `91958f76775dbe022127fc25baddb70f9b79e621a10e0397db1ad68c33f7930a` |
| `aces-core/lib/Lib.Academy.Tonescale.ctl` | `c63cc6127c594d0346f931680c1ef73a2abde5b7f7d82409e7aefdb74597d0f4` |
| `aces-core/lib/Lib.Academy.OutputTransform.ctl` | `545a95d5f44f929365aaf989b266b98c14f2ebe1e7dea2ce9d30d0213f2d1b7a` |
| `aces-core/lib/Lib.Academy.DisplayEncoding.ctl` | `1e732637fbc2d7ae907a078ef4fea8a6664b959c29d90f9b2883b61c8de30a87` |
| `aces-core/lib/Lib.Academy.ColorSpaces.ctl` | `f99680a00ef7b9a033d03c34085ceeba4f2f0ad0211e70f5aac8ef90a0d4e400` |
| `aces-output/d65/rec2100/Output.Academy.Rec2100-D65_1000nit_in_Rec2100-D65_ST2084.ctl` | `0efe33d19e504389f8c61d2d21ec504ecb58fba32de9ad7c22a70268784a023b` |
| `aces-output/d65/rec2100/Output.Academy.Rec2100-D65_2000nit_in_Rec2100-D65_ST2084.ctl` | `af2238a02aa178af773fdd82b6095501caf37f0e6b145f31d36a584b4132d9fb` |

The canonical Rec.709/D65 SceneColor is converted to ACES2065-1/D60 with the
Bradford matrix frozen in `Profiles.json` before the official forward output
transform is evaluated. Only the 1000- and 2000-nit Rec.2100/D65 forward
presets are in Feature 029 scope. The repository implementation outputs
display-linear values before Feature 029 applies its selected PQ or linear-HDR
native packer.

## Transfer and Native-Packer Authorities

- ST 2084 constants and 10,000-nit absolute normalization: SMPTE ST 2084 as
  reproduced in the pinned ACES
  `Lib.Academy.DisplayEncoding.ctl`; BT.2100 profile semantics:
  `https://www.itu.int/rec/R-REC-BT.2100/en`.
- sRGB constants: Khronos Data Format Specification 1.4,
  `https://registry.khronos.org/DataFormat/specs/1.4/dataformat.1.4.html`.
- BT.709 OETF frozen rounded constants: ITU-R BT.709 (`alpha=1.099`,
  `beta=0.018`, slope `4.5`, exponent `0.45`).
- Standard scRGB: `1.0 = 80 nits`, following Microsoft HDR/scRGB guidance:
  `https://learn.microsoft.com/windows/win32/direct3darticles/high-dynamic-range`.
- Metal EDR: `1.0 = current native reference white`, following Apple's custom
  tone-mapping and EDR guidance:
  `https://developer.apple.com/documentation/metal/performing-your-own-tone-mapping`.

PQ, scRGB80, and Metal EDR values are compared only after decoding to absolute
nits/XYZ. Raw scRGB80 and Metal EDR code values are never cross-compared.

## Project-Frozen Policies

The following are Stoner project policies rather than claims about an upstream
standard: exposure range and samples, finite-negative boundary behavior,
opaque output alpha, Extended Reinhard `Lwhite=4.0`, explicit gamma 2.2,
decoded-domain tolerance multipliers, exact-dimension evidence, and the
human-only macOS HDR visual authority. Their canonical values live in
`Config/Validation/OutputTransform/Profiles.json` and the Feature 029 contracts.
