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

Earlier local attempts showed that eight manager workers failed the 20/2
regular profile at 30,310,400 bytes of post-warm-up RSS growth and four workers
could still vary to 24,313,856 bytes on a hosted arm64 runner. The stabilized
contract uses one worker for arm64 Metal regular, four for other regular
targets, and eight for medium/hardware. Restoring four workers outside arm64
Metal preserves the previously passing Linux RSS and Intel Metal 10-minute
budget evidence. A focused arm64 candidate-revision rerun passed 20/2 in 75.980
seconds with 4,210,688 bytes of RSS growth, zero terminal owners, and
stale-handle rejection.

## Every-Root Medium Metal Closeout

Command:

`python3 .github/scripts/run_production_content_validation.py --profile medium --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json --build-root Build/Mac/Release --output Build/Validation/028/medium-metal-parallel-final --acquire-missing --timeout-seconds 1800`

Result: PASS in 1,450.659 seconds against the 1,800-second lane budget. Both
roots passed clean/warm cooking, publication validation, source-unavailable
strict loading, complete semantic equivalence, and 1,000 full lifecycle cycles
with cycles 1-20 included as warm-up. The disjoint package roots ran with a
maximum concurrency of two under the same shared deadline; summary ordering
remained the corpus manifest order.

| Package | Assets / warm reuse | Native seconds | Peak RSS | RSS growth | Owners / stale |
|---|---:|---:|---:|---:|---:|
| Khronos Lantern GLB | 37 / 37 | 302.609 | 546,226,176 | 0 | 0 / rejected |
| Khronos Sponza glTF | 189 / 189 | 1,376.784 | 334,987,264 | 14,188,544 | 0 / rejected |

Sponza remained below the 16 MiB limit by 2,588,672 bytes. Each package retained
the final seven GPU readbacks and recorded 2,000 Deferred/Forward captures.

## Artifact Revalidation

| Output | Artifact count | Manifest SHA-256 | Result |
|---|---:|---|---|
| Regular | 3,522 | `0efb0b86ae38794c3bffcd1b7baef528732460d692162e9c293ff096bfb83cd0` | PASS |
| Medium | 4,289 | `f84189a9ef28fb1f3f5ecfc92c57e4d2ad327d0cd103cd6b175ded86715f29e9` | PASS |

The revalidation command was
`python3 .github/scripts/run_production_content_validation.py --verify-only <output> --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json`.
It rehashed every indexed artifact, revalidated size and target-profile
evidence, and rejected missing or substituted content.

Key report digests:

- Regular summary: `564f15e501e1435f4fbe12dc3915aa117b8fefd26c61335e2934bd94c1568381`
- Regular native lifecycle: `687ec8a129971ecb57dc8d88fcb60a52dcb1f5ec8ecdda8f92e92c16b3ff0937`
- Medium summary: `322985859d519229a333626c25b523380cde9e2f714b8866973a3f270100006c`
- Lantern medium lifecycle: `cc9077f9f7e57a50bfcd6130617e0dd68183bb3a71484f489925c44ffb4338ee`
- Sponza medium lifecycle: `e7956427a10d7ed863793e1b82aeab026ecab98f571a78c47edb3e0fd7817fda`

## Outcome

T097 passes. Regular and medium tiers are bounded and reproducible on the local
Metal lane, Unsupported remains a structured non-success, timeout fails closed,
and consumer-side artifact revalidation succeeds. These local outputs are not a
substitute for the same-revision hosted and physical closeout required by
T112-T114.
