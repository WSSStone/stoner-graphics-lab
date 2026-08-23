# Feature 028 US2 Cook And Runtime Equivalence

Captured on 2026-08-23 from branch
`028-production-content-acceptance`, based on revision
`66a20cc42881d3747d836f9f45257c37f7f3e039` plus the current Feature 028
implementation worktree.

## Scope And Method

The bounded Khronos Lantern root and its complete Deferred shader closure were
validated against all five required production target profiles. Each target
ran 20 isolated clean imports/cooks. Canonical reports compared identities,
ordered dependencies, source and target evidence, generation manifests, and
payload evidence. An unchanged warm cook then reused all 37 eligible assets.
The published generation passed standalone validation, its authoritative
package and shader roots were made unavailable before strict manager
construction, and the strict closure was compared with development mode across
every payload family.

Metal x86_64 was cross-built and cooked by an x86_64 process under Rosetta in
an isolated workspace. This is valid offline-finalizer evidence, but Rosetta is
not accepted as physical Intel Metal hardware. Its native stage is therefore a
structured `Unsupported` result naming `macos-metal-x86_64-hardware` as the
replacement lane. Linux and Windows native stages are likewise outside this
macOS host and name their platform hardware lanes. These native results do not
replace the later final-revision hardware gates.

## Per-Target Results

| Target profile | Clean cooks | Warm reuse | Generation | Manifest digest | Clean report digest | Native on this host |
|---|---:|---:|---|---|---|---|
| Mac Metal arm64 | 20/20 | 37/37 | `f60c3069294c588e146073d736dd8cd325760c8d1645ae6c412cf58c45a709d3` | `0dae0f3f93fd6623cfacc4e6eb9bb41e41b23ef8b74c1a03e11bc5349cc04b66` | `ffdcdaf6f76c72db661fcced56c1c58e5147fb5805c8d64fbb7c2ee76647f7f7` | Passed, 20 cycles |
| Mac Metal x86_64 | 20/20 | 37/37 | `475c64f5759d59da539f09a61ee623a69a3ab556608fc7e44d3ab0ad07c73c37` | `6ef0970e24f31f92d7708f78f4a4281ad142ad5f609b46d5d27ad0d8fd0e8517` | `d66552ec6fc9d41b3ea0882474e98a08f64e8a67326b1d0dd6105cd3099f7f87` | Unsupported under Rosetta; Intel lane required |
| Mac Vulkan arm64 | 20/20 | 37/37 | `88d01dc9de176f9f532ebeb1a3110f303a7b6067583260364485ff85e140091c` | `572bdef2dfe19658edb864cb1c181d13605b06acb433f569c8485995d93c442e` | `70434364f69cb5d9dcf532abccd6c50723bfe8ca1cc5ee467a2b03bae5691d37` | Passed, 20 cycles |
| Linux Vulkan x86_64 | 20/20 | 37/37 | `27a23e2b2dee6807dfa7ff9c60634a852b7612a08364fc818bff97dba58c675d` | `99d8a724c018dd37e0e15c06c68c94f5e832be2dc01e8f4153266084f3815c70` | `dc3a34da112e6889aa4501327453498c93d73e041bb3cc6fe8c61e74fd1bf62e` | Unsupported; Linux lane required |
| Windows Vulkan x86_64 | 20/20 | 37/37 | `78a8729249b01d4aec09fe1b0cac6f2c6977623d87f783463f749a0cdda3490f` | `79ba0650d1faa376658ecc261c1924795680b1da38e7ee7058ffe2fff3c09e5c` | `2ac333a7db758da11f6c6bbdeb59078d8c49dedfdbd1ca1748b324888761f431` | Unsupported; Windows lane required |

Every row also passed standalone publication validation, source-unavailable
strict loading, zero source-participant/fallback counters, and complete
development-versus-strict semantic equivalence.

## Bounded Artifact Evidence

Each target manifest covers 3,522 bounded artifacts.

| Target | Summary SHA-256 | Artifact-manifest SHA-256 |
|---|---|---|
| Mac Metal arm64 | `d3820ad58b77198b11fee2f21b2deef18dd85f9a31209b81eb008e0b1b4fe769` | `837e2ca8228f0f5bfdfc953a92ed5680a3eb06199f85ad4099d8e5873e353598` |
| Mac Metal x86_64 | `e05a458b2ff7fc7c312d4eaf1a14351b4ada16b4b11d1fb8012446c9b50f8b0e` | `5275ea3ec8cd01cdb3f33a2d9c86f790b840a47a57eafa73fc365994436f1eb2` |
| Mac Vulkan arm64 | `6433c9bd0537c50432a3e6c2b003064c2bfd20e557dccdf0282004dbac09302f` | `cb64d6f974a2fedcacdd7e6da68643d4cf08a31fe7c53009369eae45b94aaee0` |
| Linux Vulkan x86_64 | `e383355fd0d152b5f92a99e7510a6f54df83a2aecf7a637d7f4987aef3c03ac5` | `3be9739d5e1fba2d21cd2205587a928c7ebbed2e7d2af272e60fe507c92c393b` |
| Windows Vulkan x86_64 | `3b9457bc41d16abd01f7a0c0d22cdc56dc6e99357a8a70fbef6911b65e6e1ff9` | `0f605893754615e4404da463236785e81ad11370b7ff1b3f17077ab13cae6e81` |

## Mutation And Corruption Regression

| Gate | Result |
|---|---|
| `production-content-cook-graph` plus `asset-cooker-snapshot` | Passed, 11 checks; source mutation failed without publication and preserved the prior generation and `Current.json` |
| `production-content-strict-runtime` plus `production-content-equivalence` | Passed, 26 checks; complete closure equivalence plus missing, malformed, corrupt, substituted, unexpected, cancellation, and exact failure-catalog cases |
| Python production validation runner | Passed, 29 tests; includes five-target routing, artifact substitution rejection, structured Unsupported aggregation, Metal cook-host preflight, and Rosetta native classification |

## Outcome

T050, T051, and T054 pass. The regular root has deterministic, reusable,
standalone-valid, source-independent, semantically equivalent cooked output for
every required target profile. Platform-native and final-revision hardware
coverage remains intentionally assigned to T112-T114.
