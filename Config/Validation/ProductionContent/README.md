# Production Content Validation Profiles

`Regular.json`, `Medium.json`, and `Hardware.json` are version-2 strict workload
contracts consumed by the shared local/CI runner. Unknown fields, profile
overrides, and caller-selected warm-up counts are rejected.

`DeviceClasses.json` maps a canonical observed capability signature to exactly
one stable class. Marketing device names and driver versions are observations,
not class keys. Registry records are sorted by `deviceClass`; duplicate class
tokens or complete signatures are invalid.

Regular uses cycles 1-2 of 20 as included warm-up. Medium and hardware use
cycles 1-20 of 1,000. RSS growth is measured from the sample immediately after
warm-up to the terminal sample. The 16 MiB result is `observed` on GitHub-hosted
and local-diagnostic runs and is `required` only on a maintainer-local Metal run
that passed the profile-owned preflight.

Execution class is derived from repository policy and validated provenance;
there is no generic `--execution-class` override. `github-hosted` owns deterministic,
functional, native, exact-cycle, capture/readback, owner, stale-handle, and
operational-timeout results. Its RSS, task-VM, allocator, peak, and elapsed
values remain bounded observations and cannot turn complete functional work
into failure or success. `local-diagnostic` has no closeout authority.
`maintainer-local-metal` is available only through the explicit
`--local-metal-authority` hardware command after native arm64 macOS, exact
target/device class, non-Rosetta execution, process-level exclusive lock, clean
committed revision, default allocator, sample protocol, and window/readback
preflight. It owns the required 16 MiB RSS and accepted image decisions. The
flag cannot authorize Vulkan or another target.

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
| Medium | Lantern and hash-pinned Sponza | 1,000 / 20 | 90 minutes complete / 80 minutes native | Isolated hosted Intel Metal package lanes; external package cache may be reused only after hash verification |
| Hardware | Lantern and hash-pinned Sponza | 1,000 / 20 | 60 minutes | Maintainer-local native arm64 macOS Metal with an application display surface |

The runner does not accept caller overrides for cycle or warm-up boundaries.
`Unsupported` is a structured non-success result and always identifies the
missing prerequisite plus the replacement command. An incomplete maintainer-local
Metal preflight is `Unsupported`, never an ordinary local or hosted pass. Hardware runs set
`STONER_PRODUCTION_VISIBLE=1`; the captured bytes come from the application's
own swapchain/drawable surface, never from a full-screen capture API.

All classes use the normal production allocator. Validation must not set
`MallocMediumZone`, `MallocNanoZone`, `MallocMaxMagazines`,
`MallocSpaceEfficient`, or `MALLOC_ARENA_MAX` to manufacture RSS behavior.
Operational timeouts fail when required work is incomplete; finishing more
slowly on a completed diagnostic replay is an observation, not deterministic
correctness identity.

Windows Vulkan and macOS Vulkan physical qualification are deferred to a future
hardware-lab phase. Feature 028 does not auto-queue self-hosted workflows when
no such runner exists.

The external corpus is staged under
`Content/ProductionAcceptance/External/Sponza`, which is ignored as generated
input. CI may cache that directory using the canonical corpus-manifest digest,
but `verify_production_corpus.py` still validates inventory, size, and SHA-256
before import or cooking. Outputs, DDC entries, published generations, captures,
and timing/RSS observations remain below ignored `Build/Validation/028/` paths.
