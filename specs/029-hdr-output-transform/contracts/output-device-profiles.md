# Contract: Output Device Profiles

## Renderer Profile Matrix

| Profile ID | Viewing/Tone Transform | Display-linear Domain | Encoding | Storage | Native Color Space | Comparison |
|---|---|---|---|---|---|---|
| `Sdr.sRGB.v1` | Selected SDR curve | Rec.709/D65, 100-nit policy | explicit sRGB | UNorm8 | sRGB nonlinear | decoded linear Rec.709 |
| `Sdr.BT709.v1` | Selected SDR curve | Rec.709/D65, 100-nit policy | BT.709 OETF | UNorm8 | BT.709 nonlinear | decoded linear Rec.709 |
| `Sdr.ExplicitGamma22.v1` | Selected SDR curve | Rec.709/D65, 100-nit policy | gamma 2.2 | UNorm8 | pass-through/declared SDR | decoded linear Rec.709 |
| `Hdr.PQ.Rec2020.1000.v1` | ACES 2 1000 nit | Rec.2020/D65 absolute nits | ST-2084 | Packed10 UNorm | HDR10 ST-2084 | decoded nits/XYZ |
| `Hdr.PQ.Rec2020.2000.v1` | ACES 2 2000 nit | Rec.2020/D65 absolute nits | ST-2084 | Packed10 UNorm | HDR10 ST-2084 | decoded nits/XYZ |
| `Hdr.Linear.1000.v1` | ACES 2 1000 nit | Rec.709/D65 absolute nits | scRGB80 or Metal EDR | RGBA16F | extended-linear sRGB | decoded nits/XYZ |
| `Hdr.Linear.2000.v1` | ACES 2 2000 nit | Rec.709/D65 absolute nits | scRGB80 or Metal EDR | RGBA16F | extended-linear sRGB | decoded nits/XYZ |

The schema file is the machine-readable profile contract. Target peak belongs
to Renderer policy and evidence, not to `ERHIPresentationColorSpace`.

## SDR Curve Rules

- Default resolves to `Sdr.KhronosPbrNeutral.v1` and is always recorded.
- `Sdr.NarkowiczAcesFit.v1` is named as a fit and never reported as Academy
  ACES conformance.
- `Sdr.ExtendedReinhardRec709.v1` freezes `Lwhite=4.0`; no scene-derived white.
- All receive non-negative finite exposed linear Rec.709 and emit finite
  display-linear Rec.709 in `[0,1]` for output encoding.

## HDR Rules

- HDR uses only `Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1` with the selected
  1000/2000-nit preset; no SDR curve executes first.
- PQ is absolute: ST-2084 normalizes against 10,000 nits, not target peak.
- Standard scRGB decodes with 80 nits per 1.0. Metal EDR decodes with the
  same-generation native reference white; raw values are not cross-compared.
- Metadata describes content/mastering intent but neither owns encoding nor
  proves visual correctness or achieved display peak. Vulkan may map that
  intent to `VK_EXT_hdr_metadata` when available. Metal does not map it to
  `CAEDRMetadata` for either HDR path.
- On Metal PQ, Renderer produces final Rec.2020/ST-2084 code values and the
  layer uses `BGR10A2Unorm`, ITU-R 2100 PQ, EDR opt-in, and
  `EDRMetadata=nil`; Core Animation color management is allowed. On Metal EDR,
  `EDRMetadata` is also nil and Renderer owns native-reference-white linear
  packing.

## Exactly-Once Transfer Ownership

Renderer performs the declared gamut conversion, luminance mapping, and output
encoding into FinalOutput. The resolved native format/color space interprets
those bytes/values and MUST NOT perform a second application-defined shader
tone map or transfer. Metal PQ Core Animation color management is the single
declared display-adaptation disposition and must be reported; it does not
change the Renderer code-value contract and does not enable system tone
mapping. Reports record requested and actual state separately and reject any
mismatch.

## Frozen Non-Visual Tolerances

- CPU double-oracle components use
  `max(1e-10,1e-10*abs(expected))`.
- SDR decoded linear channels use `2/255` absolute error.
- HDR decoded linear RGB nits components use
  `E=max(0.02,0.0025*max(1,abs(expected)),M*Qnative)`, where `Qnative` is the
  larger adjacent native-code decoded step bracketing the expected value,
  `M=1.5` for packed-10 PQ, and `M=2.0` for FP16 scRGB/Metal EDR.
- XYZ component tolerance is the absolute propagation of the three RGB `E`
  values through the frozen conversion matrix plus `1e-6`.
- Non-finite comparison values always fail. These are numerical conformance
  bounds only and are never HDR perceptual scores.

## Unsupported Behavior

If the exact format/color-space/metadata/EDR requirements are unavailable, the
profile result is `Unsupported`. There is no fallback to SDR, a different peak,
or a different encoding. System tone mapping is forbidden on both Metal HDR
paths. Windows may compile and exercise
deterministic profile math, but Feature 029 emits no Windows HDR validation or
authority result.
