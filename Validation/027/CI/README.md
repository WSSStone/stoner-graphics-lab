# Feature 027 CI Evidence

Downloaded artifacts remain in the ignored `downloaded/` directory. Hosted
Metal-device probes are useful native conformance evidence, but do not replace
the broader, fail-on-unavailable self-hosted hardware acceptance required by
T122 and T123.

## Hosted Matrix Run 32382290743

- Workflow: `Feature 027 Metal Backend`
- Event: branch push
- Revision: `3b9b47e7c094a2b78ec1726f375ddf0608eacfa7`
- Attempt: 1
- Started: `2026-08-20T14:47:53Z`
- Completed: `2026-08-20T15:08:42Z`
- Conclusion: **success (10/10 jobs)**
- Run: <https://github.com/WSSStone/stoner-graphics-lab/actions/runs/32382290743>

Windows x86_64 Debug and strict Release; Linux x86_64 Debug, strict Release,
ASan/UBSan, and TSan; and macOS arm64/x86_64 Debug and strict Release all
passed. Every hosted platform completed twenty-repeat deterministic MSL
derivation and deterministic failure diagnostics. Linux sanitizer lanes also
passed their lifecycle gates.

### Exact-Revision Artifacts

These are GitHub's SHA-256 digests for the finalized artifact archives.

| Artifact | SHA-256 |
|---|---|
| `metal-backend-windows-x86_64-debug-1` | `14d2fdfe20f379485832b9350235cbd745c9df5a481439953c1df41e62a66a36` |
| `metal-backend-windows-x86_64-release-1` | `3bfa0931177da4b913f67e53b6d6418d8793f006db8d16aef1041a2e824330ec` |
| `metal-backend-linux-x86_64-debug-1` | `cd9b5c4a32ce27af982e55db3de7826ebdd32d7684721f04a7b7322333dd5e81` |
| `metal-backend-linux-x86_64-release-1` | `61f44e9c91870ff577c95474af2b2df64922d89f1724300bf48ccaca30a7f555` |
| `metal-backend-linux-asan-ubsan-1` | `1671d93f4baf26db8b48adf4bd8cdb004bb155cc94f3d3dcf9228ca139852ad8` |
| `metal-backend-linux-tsan-1` | `dbdf6db207ce8203e56350eee635aaca4df187e03cfe11937a6b762cc24b3f93` |
| `metal-backend-macos-arm64-debug-1` | `9676ba01e05181fec2762874d59f159cd5430940bb9dd68912a9eb04a96eff2e` |
| `metal-backend-macos-arm64-release-1` | `7d9a106175aad5f0a6c0a3b914a3db7c77aa2ed8b21271a87e3cedd6dfe27921` |
| `metal-backend-macos-x86_64-debug-1` | `4cce3357ce183099973eb74041fbb42bb86bc8d9f1f6b7551abaf57d5c521ebf` |
| `metal-backend-macos-x86_64-release-1` | `6411bd976389e1b84ba8a84c16ff439934061b97e5b13c5668ba6c3c489493cc` |

### Hosted Native Probe Classification

Both arm64 jobs ran on `macos26` and passed `metal-native-conformance` on an
`Apple Paravirtual device` with capability digest
`09a69ee64cc058d334ad073ffd8c086014a1671bb971f64fbf4f7a5a9d825bac`.
Both x86_64 jobs ran on `macos26` and passed the same workload on an
`Apple Paravirtual device` with capability digest
`73e31afc30b78af97fdb9bcfbcf06e58128ba873b28ee82a83a03d7687317399`.
Windows and Linux correctly skipped native Metal execution. Runner identity,
derivation, failure, lifecycle, and native reports were downloaded and
inspected under the ignored `downloaded/` directory.

## Local M4 Pro Visible Acceptance

The exact revision passed a 3,000-frame real-window run with 40 observed
presentation recoveries for 20 requested resize/minimize/restore cycles,
190.600 ms to first present, 20 RSS samples, 114,688 bytes of final-minus-
baseline RSS growth, and zero final live objects. The accepted PNG contains
only the stable `StonerDemo` window and a correctly oriented RGB triangle.

| Evidence | SHA-256 |
|---|---|
| `Validation/027/captures/visible-acceptance.json` | `9373a16e61c6b6e4103ca1d374ef0d3f1b4f79915c7366edff84ec0d5973c959` |
| `Validation/027/captures/visible-metal-arm64.png` | `9ba5d6e4494910062909f9364f4ffc9c1772ce18b595fa1342c5e61fa68ed7db` |

## Checked-In Report Validation

All eleven checked-in JSON reports passed both the runner's canonical digest
validator and Draft 2020-12 validation against
`metal-validation-report.schema.json`.

| Report | SHA-256 |
|---|---|
| `us1-rhi-conformance.json` | `b2d9eaab4d11fb423077d0dcd66535a08721d5073025425755f293862a8161e9` |
| `us2-presentation-smoke.json` | `da53d5b775ef57628189be5ac43673a1acbe9d425635ab652c09bf97e492854e` |
| `us3-derivation-determinism.json` | `f719c55932e22f376d939cb7903e6f1a3b7224d2d15b86e13384579101087a28` |
| `us3-native-cook-determinism.json` | `d66f3f30265e2df42d154d7b37b2145de1f41a8f4ee9a91614d75e339d5261ad` |
| `us4-metal-deferred.json` | `98c8f6541a14a051874ac17cb01591629ddba1a7a1fafe11821fda5f48b85103` |
| `us4-metal-triangle.json` | `e47d1d775b0266eba963bfbafa8ded853c4b6180b2dacbbc9595ca1fc6864f4e` |
| `us4-metal-vulkan-comparison.json` | `08dbd359bc3915bdb08376a656a58796cf9534ae17273b56a52f848297d23ef4` |
| `us4-vulkan-regression.json` | `e2cef265dee38a7ae08105b4b6d1318c0a4f74aa3df15684ea4f2c9457c79bd6` |
| `us5-failure-determinism.json` | `6f49bc38ef4809d9bfbd33b7ccd485821e2d5ad88c17e6b43391c68b600df3a8` |
| `us5-lifecycle-stress.json` | `2e67428895e099678d0b37072cc32edfa05feb9bb9c3250191c78dbabe4d6eb2` |

## Hosted Matrix Run 32355696832

- Workflow: `Feature 027 Metal Backend`
- Event: branch push
- Revision: `de423cf0e28b90410184bb9ab7138987d40231a5`
- Attempt: 1
- Started: `2026-08-20T09:47:45Z`
- Completed: `2026-08-20T10:06:48Z`
- Conclusion: **success (10/10 jobs)**
- Run: <https://github.com/WSSStone/stoner-graphics-lab/actions/runs/32355696832>

The run passed Windows x86_64 Debug and strict Release; Linux x86_64 Debug,
strict Release, ASan/UBSan, and TSan; and macOS arm64/x86_64 Debug and strict
Release build/cook jobs. Every supported hosted job also passed twenty-repeat
deterministic MSL derivation and deterministic failure diagnostics. The Linux
sanitizer jobs passed 10,000-iteration lifecycle validation.

### Artifacts

Digests below are the SHA-256 values reported by GitHub Actions for the
finalized artifact archives.

| Artifact | SHA-256 |
|---|---|
| `metal-backend-windows-x86_64-debug-1` | `e34719d6b5f3e45d19d38b9b3301265f3a2dda9e82faed1b456e3c19df05c6d6` |
| `metal-backend-windows-x86_64-release-1` | `10fedc0284cfbc9d38554922d564a103f6588a777aa319dd6db912fa83d2d99f` |
| `metal-backend-linux-x86_64-debug-1` | `74191e9d4762260cb437aad79d9449704d701b58e4a14970448e0aee276408f6` |
| `metal-backend-linux-x86_64-release-1` | `70536d78097d25369b74aeb7b9a6312ff1e1d8f0c371f47188df5a3b4393f172` |
| `metal-backend-linux-asan-ubsan-1` | `c4f4897b0491fe796e1595f112062ce3c4f2b16d44600c535e20100ab7ace5ad` |
| `metal-backend-linux-tsan-1` | `ec3af5a2fcdf5f2a46c3575e25174e9ed5825cc8a7be083fca0c51d8b808615a` |
| `metal-backend-macos-arm64-debug-1` | `a1b403bac4ee6ff32911edf4ced7107fc6927605b7a0b7b07b776ff6bb0e34c5` |
| `metal-backend-macos-arm64-release-1` | `31407706d639435a228e01a6c91de91f062276dd286aa91027b4cec81d31565b` |
| `metal-backend-macos-x86_64-debug-1` | `2feccaa064ffb46f4294ca56f4bb09befb2b8ce649bfb94c6d55b9e934a981f4` |
| `metal-backend-macos-x86_64-release-1` | `0cb3f0f25489ad9aed49e9fe5f1b3e8ced27736fad2553594496758e41dd6c4e` |

### Hosted Native Probes

All four hosted macOS native-offscreen reports passed and recorded real Metal
device identities. The arm64 jobs used an `Apple Paravirtual device` with
capability digest
`09a69ee64cc058d334ad073ffd8c086014a1671bb971f64fbf4f7a5a9d825bac`;
the x86_64 jobs used an `Apple Paravirtual device` with capability digest
`73e31afc30b78af97fdb9bcfbcf06e58128ba873b28ee82a83a03d7687317399`.
Both architectures produced `passed` `metal-native-conformance` reports at the
`native-offscreen` tier.

These hosted probes cover device/resource/queue conformance and architecture-
matched shader cooking. They do not contain the full triangle/deferred
comparison, visible presentation, failure, lifecycle, and local-hardware
cross-check bundle required by the self-hosted hardware workflow.

## Remaining Hardware Acceptance

Run `32377219273` was created by the branch-scoped push trigger at revision
`9cec77d9bd9881c0dbe8df69669f895f4d1406a3`. Its `Native Metal arm64` and
`Native Metal x86_64` jobs remain queued because the repository currently has
zero registered self-hosted runners.

| Lane | Required labels | Gap owner | Status |
|---|---|---|---|
| arm64 | `self-hosted`, `macOS`, `metal`, `arm64` | Repository maintainer | Queued; no matching runner |
| x86_64 | `self-hosted`, `macOS`, `metal`, `x86_64` | Repository maintainer | Queued; no matching runner |

Diagnostic commands:

```bash
gh api repos/WSSStone/stoner-graphics-lab/actions/runners
gh run view 32377219273 --json databaseId,headSha,status,conclusion,jobs,url
```

Follow-up gate: register GPU-capable Macs with the exact labels above, allow
both already-queued jobs to finish, require success, download both
`metal-hardware-*` artifacts, and create the checked-in `native-arm64.json` and
`native-x86_64.json` acceptance reports. Until then T122, T123, T126, and T127
remain open; hosted paravirtual probes and the local M4 Pro run do not waive the
missing Intel-hardware evidence.
