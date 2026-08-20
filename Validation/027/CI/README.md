# Feature 027 CI Evidence

Downloaded artifacts remain in the ignored `downloaded/` directory. Hosted
Metal-device probes are useful native conformance evidence, but do not replace
the broader, fail-on-unavailable self-hosted hardware acceptance required by
T122 and T123.

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

The required `Feature 027 Metal Hardware` arm64 and x86_64 self-hosted jobs have
not been dispatched from this branch. T121-T125 remain open until both
fail-on-unavailable hardware lanes, the local M4 Pro cross-check, visible
acceptance, and checked-in report-schema validation are complete.
