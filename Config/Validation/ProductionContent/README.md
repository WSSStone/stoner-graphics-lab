# Production Content Validation Profiles

`Regular.json`, `Medium.json`, and `Hardware.json` are strict workload
contracts consumed by the shared local/CI runner. Unknown fields, profile
overrides, and caller-selected warm-up counts are rejected.

`DeviceClasses.json` maps a canonical observed capability signature to exactly
one stable class. Marketing device names and driver versions are observations,
not class keys. Registry records are sorted by `deviceClass`; duplicate class
tokens or complete signatures are invalid.

Regular uses cycles 1-2 of 20 as included warm-up. Medium and hardware use
cycles 1-20 of 1,000. RSS growth is measured from the sample immediately after
warm-up to the terminal sample and must not exceed 16 MiB.

All Feature 028 profiles reference target descriptions under
`Config/AssetCooker/Profiles/Production/`. These profiles retain the established
platform, shader, texture-semantic, mip, and target-capability contracts while
selecting the KTX2 `uncompressed` portable fallback. The production corpus still
passes through `cooker.ktx2`, emits validated KTX2 payloads, and exercises strict
cooked loading and Renderer realization. ETC1S and UASTC encoder correctness
remain independently gated by Feature 022; serial WAMR encoder throughput is not
part of the regular production-content acceptance budget.

## Tier Matrix

| Profile | Corpus | Cycles / warm-up | Budget | Required environment |
|---|---|---:|---:|---|
| Regular | Checked-in Lantern | 20 / 2 | 10 minutes | Windows, Linux, or macOS target-capable host; headless/software-native evidence where applicable |
| Medium | Lantern and hash-pinned Sponza | 1,000 / 20 | 30 minutes | Target-capable closeout or scheduled runner; external package cache may be reused only after hash verification |
| Hardware | Lantern and hash-pinned Sponza | 1,000 / 20 | 60 minutes | Physical Windows Vulkan or macOS Vulkan/Metal device with an application display surface |

The runner does not accept caller overrides for cycle or warm-up boundaries.
`Unsupported` is a structured non-success result and always identifies the
missing prerequisite plus the replacement lane. Hardware runs set
`STONER_PRODUCTION_VISIBLE=1`; the captured bytes come from the application's
own swapchain/drawable surface, never from a full-screen capture API.

The external corpus is staged under
`Content/ProductionAcceptance/External/Sponza`, which is ignored as generated
input. CI may cache that directory using the canonical corpus-manifest digest,
but `verify_production_corpus.py` still validates inventory, size, and SHA-256
before import or cooking. Outputs, DDC entries, published generations, captures,
and timing/RSS observations remain below ignored `Build/Validation/028/` paths.
