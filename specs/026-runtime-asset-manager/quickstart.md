# Quickstart: Runtime Asset Manager

Commands are run from the repository root. They become executable as the
corresponding implementation milestones land.

## 1. Build

```bash
conda run -n godot scons -j8 config=debug strict=1
conda run -n godot scons -j8 config=release strict=1
```

Expected macOS test binary:
`Build/Mac/{Debug,Release}/Tests/StonerTest`. Windows uses `Win64` and
`.exe`; Linux uses `Linux`.

## 2. Contract And Architecture Gates

```bash
conda run -n godot python Tests/verify_runtime_asset_manager.py --root .
conda run -n godot python Tests/verify_asset_layer.py
conda run -n godot python Tests/verify_architecture.py
conda run -n godot python -m unittest Tests/test_verify_runtime_asset_manager.py Tests/test_verify_asset_layer.py Tests/test_verify_architecture.py
```

Expected: no Asset dependency on Tools/RHI/Renderer/Application/Backend, no
native type in public Asset headers, and complete FR/SC contract coverage.

## 3. Focused Debug Suites

```bash
Build/Mac/Debug/Tests/StonerTest --suite asset-manager-contract --suite asset-manager-request --suite asset-manager-dependency --suite asset-manager-development --suite asset-manager-cooked --suite asset-manager-cache --suite asset-manager-completion --suite asset-manager-lifetime --suite asset-manager-concurrency --suite asset-manager-generation-lease
```

Expected: all suites pass without a window, GPU, Vulkan loader, or network.

## 4. Development/Cooked Equivalence

Create a representative generation with Feature 025, then run the same typed
requests through both modes:

```bash
conda run -n godot python .github/scripts/run_runtime_asset_manager_validation.py --root . --tests Build/Mac/Release/Tests/StonerTest --profile Config/AssetCooker/Profiles/Mac-Vulkan.json --work Build/Feature026Validation/local-release --output Validation/026/reports/local-release.json --repetitions 20 --timeout-seconds 1200
```

Expected: all representative 021-024 payload families agree semantically;
cooked mode records zero source resolver/importer invocations.

## 5. Mutation, Cancellation, And Completion

```bash
Build/Mac/Debug/Tests/StonerTest --suite asset-manager-source-mutation --suite asset-manager-cancellation --suite asset-manager-completion --suite asset-manager-shutdown
```

Expected:

- every pre-publication source mutation fails without retry/partial cache;
- one cancelled coalesced caller does not cancel others;
- callbacks run only on the pump thread in enqueue order;
- recursive pump is rejected;
- every accepted request is terminal after shutdown.

## 6. Cross-Process Reader Lease

```bash
Build/Mac/Debug/Tests/StonerTest --suite core-file-lease --suite asset-manager-generation-lease
```

Expected: shared readers coexist; exclusive maintenance cannot acquire while a
reader lives; normal release and forced subprocess exit permit later exclusive
acquisition; Feature 025 exclusive leases still pass.

## 7. Stress And Performance

```bash
Build/Mac/Release/Tests/StonerTest --suite asset-manager-stress --suite asset-manager-benchmark --asset-manager-benchmark-profile reference --asset-manager-benchmark-report Validation/026/reports/performance-m4-pro.txt
```

Reference gates on Apple M4 Pro:

| Workload | Limit |
|---|---:|
| Admit/coalesce 8 requests, excluding physical load | <=5 ms |
| Pre-bound index, empty payload cache, 1,000-Asset/5,000-edge graph with result handles retained | <=2 s |
| Pump 10,000 pre-reserved no-op completions | <=50 ms |
| 10,000 load/share/release cycles | <=30 s |

The stress suite also performs at least 100 concurrent startup/shutdown cycles
and verifies zero surviving manager work, stale alias, or retained cache entry.
Hosted CI uses hard 4x smoke ceilings.

## 8. Sanitizers

```bash
conda run -n godot scons config=debug strict=1 sanitizers=address,undefined
Build/Linux/Debug/Tests/StonerTest --suite asset-manager

conda run -n godot scons config=debug strict=1 sanitizers=thread
Build/Linux/Debug/Tests/StonerTest --suite asset-manager-concurrency --suite asset-manager-lifetime --suite asset-manager-generation-lease
```

Run on Linux locally or through the Feature 026 GitHub Actions jobs.

## 9. Read-Only Publication And Coordination Root

The cooked fixtures expose a read-only publication tree and a separate writable
coordination tree. Run:

```bash
Build/Mac/Debug/Tests/StonerTest --suite asset-manager-generation-lease --suite asset-manager-generation-lease-process
```

Expected: runtime never modifies publication content; absent/read-only/aliased
coordination roots fail before admission; shared readers and later exclusive
maintenance use the same publication namespace.

## 10. Full Regression

```bash
conda run -n godot python .github/scripts/run_runtime_asset_manager_validation.py --root . --tests-debug Build/Mac/Debug/Tests/StonerTest --tests-release Build/Mac/Release/Tests/StonerTest --profile Config/AssetCooker/Profiles/Mac-Vulkan.json --work Build/Feature026Validation/full-regression --output Validation/026/reports/regression.txt --full-regression --timeout-seconds 1200
```

Expected: all pre-existing and Feature 026 suites pass. Record the normalized
runner summary in `Validation/026/reports/regression.txt`.

## 11. GitHub Actions And Artifacts

```bash
gh workflow run feature-026-runtime-asset-manager.yml --ref 026-runtime-asset-manager
gh run list --workflow feature-026-runtime-asset-manager.yml --branch 026-runtime-asset-manager --limit 1
gh run watch RUN_ID --exit-status
gh run download RUN_ID --dir Validation/026/CI/downloaded
gh run view RUN_ID --json databaseId,headSha,status,conclusion,jobs
```

Replace `RUN_ID` with the ID printed by `gh run list`. Expected: three Debug,
three strict Release, ASan/UBSan, and TSan jobs pass; all eight artifacts are
downloaded and their digests recorded in `Validation/026/CI/README.md`.

## 12. Final Gate

```bash
git diff --check
rg -n "NEEDS CLARIFICATION|TODO|TBD" \
  specs/026-runtime-asset-manager/spec.md \
  specs/026-runtime-asset-manager/plan.md \
  specs/026-runtime-asset-manager/research.md \
  specs/026-runtime-asset-manager/data-model.md \
  specs/026-runtime-asset-manager/contracts
git status --short
```

Expected: no unresolved planning marker, malformed whitespace, unexplained
artifact, or untracked validation output.
