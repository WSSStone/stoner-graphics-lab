# Feature 028 US4 Tiered Acceptance

## Scope

This report records the local macOS arm64 tier-contract, workflow, regular, and
medium gates for the current uncommitted Feature 028 worktree based on inherited
revision `66a20cc42881d3747d836f9f45257c37f7f3e039`. Final-revision hosted and
physical-hardware closeout remains tracked by T112-T114.

## Contract And Automation Gates

| Gate | Command | Result |
|---|---|---|
| Profile, Unsupported, timeout, failure catalog, determinism, and artifact substitution | `python3 .github/scripts/test_run_production_content_validation.py` | PASS, 30 tests |
| Relevant-path, schedule, dispatch, producer/consumer, and digest-revalidation workflow contracts | `python3 .github/scripts/test_production_content_workflows.py` | PASS, 4 tests |
| Strict Release build | `conda run -n godot scons config=release strict=1 -j8` | PASS |

The runner tests intentionally print argparse usage for rejected determinism
boundaries and a structured timeout stage. Those messages are expected test
evidence, not gate failures.

## Regular Local Metal

Command:

`python3 .github/scripts/run_production_content_validation.py --profile regular --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json --build-root Build/Mac/Release --output Build/Validation/028/regular-metal-us4-local-v4 --timeout-seconds 600`

Result: PASS in 46.052 seconds against the 600-second budget.

| Evidence | Value |
|---|---:|
| Clean equivalent cooks | 20 |
| Maximum clean cook | 7.658 seconds |
| Warm reuse | 37 / 37 |
| Generation | `f60c3069294c588e146073d736dd8cd325760c8d1645ae6c412cf58c45a709d3` |
| Native lifecycle | 20 cycles, cycles 1-2 included warm-up |
| Native duration | 8.915 seconds |
| Peak RSS | 619,216,896 bytes |
| Post-warm-up RSS growth | 5,144,576 bytes |
| Terminal owners / stale handle | 0 / rejected |
| GPU captures / retained readbacks | 40 / 7 |

An earlier local attempt used eight manager workers for the 20/2 regular
profile and correctly failed at 30,310,400 bytes of post-warm-up RSS growth.
The fixed contract uses four workers for regular and eight for medium/hardware;
the passing run above is authoritative.

## Every-Root Medium Metal Closeout

Command:

`python3 .github/scripts/run_production_content_validation.py --profile medium --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json --build-root Build/Mac/Release --output Build/Validation/028/medium-metal-closeout-local-v3 --timeout-seconds 1800`

Result: PASS in 1,724.162 seconds against the 1,800-second lane budget. Both
roots passed clean/warm cooking, publication validation, source-unavailable
strict loading, complete semantic equivalence, and 1,000 full lifecycle cycles
with cycles 1-20 included as warm-up.

| Package | Assets / warm reuse | Native seconds | Peak RSS | RSS growth | Owners / stale |
|---|---:|---:|---:|---:|---:|
| Khronos Lantern GLB | 37 / 37 | 260.393 | 542,261,248 | 0 | 0 / rejected |
| Khronos Sponza glTF | 189 / 189 | 1,362.027 | 332,562,432 | 16,613,376 | 0 / rejected |

Sponza remained below the 16 MiB limit by 163,840 bytes. Each package retained
the final seven GPU readbacks and recorded 2,000 Deferred/Forward captures.

## Artifact Revalidation

| Output | Artifact count | Manifest SHA-256 | Result |
|---|---:|---|---|
| Regular | 3,522 | `0efb0b86ae38794c3bffcd1b7baef528732460d692162e9c293ff096bfb83cd0` | PASS |
| Medium | 4,288 | `0ff719304444d704418d0f177d089b3f46f6028c47a4c676bff85844f9752194` | PASS |

The revalidation command was
`python3 .github/scripts/run_production_content_validation.py --verify-only <output> --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json`.
It rehashed every indexed artifact, revalidated size and target-profile
evidence, and rejected missing or substituted content.

Key report digests:

- Regular summary: `564f15e501e1435f4fbe12dc3915aa117b8fefd26c61335e2934bd94c1568381`
- Regular native lifecycle: `687ec8a129971ecb57dc8d88fcb60a52dcb1f5ec8ecdda8f92e92c16b3ff0937`
- Medium summary: `f0f65ca90c6527aefdfb1ab7ff1553eda57cebd324d62b00eb17e5021aacb0d6`
- Lantern medium lifecycle: `14d2092210169b860aa8d9713331994a53cb21287adb06380c96a77b356c9a0d`
- Sponza medium lifecycle: `4a0674742afcc573e0363728fcf5f515003c2b17261d5495a4380caed501f1c2`

## Outcome

T097 passes. Regular and medium tiers are bounded and reproducible on the local
Metal lane, Unsupported remains a structured non-success, timeout fails closed,
and consumer-side artifact revalidation succeeds. These local outputs are not a
substitute for the same-revision hosted and physical closeout required by
T112-T114.
