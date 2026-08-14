# Quickstart: Asset Cooker, Manifest & Derived Data

These commands define the Feature 025 implementation and acceptance workflow.
They become executable as the corresponding milestones land. Run from the
repository root.

## 1. Environment And Contract Syntax

```bash
conda run -n godot python --version
conda run -n godot scons --version
python3 -m json.tool specs/025-asset-cooker-derived-data/contracts/target-profile.schema.json
python3 -m json.tool specs/025-asset-cooker-derived-data/contracts/manifest.schema.json
python3 -m json.tool specs/025-asset-cooker-derived-data/contracts/report.schema.json
```

Expected:

- SCons is 4.10.1 or newer according to the repository build gate.
- Each schema parses as JSON.
- The `godot` environment is reused for project builds; Feature 025 adds no
  Python runtime package.

## 2. Build Debug And Release

```bash
conda run -n godot scons -j8
conda run -n godot scons -j8 config=release
```

Expected executables on macOS:

```text
Build/Mac/Debug/Tools/AssetCooker/StonerAssetCooker
Build/Mac/Debug/Tests/StonerTest
Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker
Build/Mac/Release/Tests/StonerTest
```

Windows uses `Win64` and `.exe`; Linux uses `Linux`.

## 3. Focused Contract And Tool Tests

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite asset \
  --suite asset-material-shader \
  --suite asset-static-mesh \
  --suite asset-static-model \
  --suite renderer-texture \
  --suite asset-cooker-codec \
  --suite asset-cooker-graph \
  --suite asset-cooker-ddc \
  --suite asset-cooker-publication \
  --suite asset-cooker-determinism \
  --suite asset-cooker-concurrency \
  --suite asset-cooker-cli \
  --suite asset-cooker-report \
  --suite asset-cooker-workflow

conda run -n godot python -m unittest \
  Tests/test_verify_asset_cooker_contracts.py
```

Expected: all suites pass. The focused tests must not require a GPU, window, or
network service.

## 4. Plan Without Mutation

```bash
rm -rf Saved/Feature025Quickstart
mkdir -p Saved/Feature025Quickstart
cp -R Tests/Fixtures/AssetCooker/Representative \
  Saved/Feature025Quickstart/Source

Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker plan \
  --source-root Saved/Feature025Quickstart/Source \
  --cook-all \
  --target-profile Config/AssetCooker/Profiles/Mac-Vulkan.json \
  --output Saved/Feature025Quickstart/Cooked \
  --ddc Saved/Feature025Quickstart/DerivedDataCache \
  --workers 4 \
  --normalized-report \
  --report Saved/Feature025Quickstart/plan.json
```

Expected:

- exit code 0;
- deterministic discovery/root/graph order and expected miss reasons;
- no `Entries`, `Quarantine`, `Generations`, `Staging`, or `Current.json`
  created under the supplied DDC/output roots.

## 5. Clean Cook And Publish

```bash
Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker cook \
  --source-root Saved/Feature025Quickstart/Source \
  --cook-all \
  --target-profile Config/AssetCooker/Profiles/Mac-Vulkan.json \
  --output Saved/Feature025Quickstart/Cooked \
  --ddc Saved/Feature025Quickstart/DerivedDataCache \
  --clean \
  --workers 4 \
  --lease-timeout-ms 30000 \
  --normalized-report \
  --report Saved/Feature025Quickstart/clean-cook.json
```

Expected:

- exit code 0;
- one complete immutable generation and canonical `Current.json`;
- one manifest record for every reachable typed output;
- all payloads pass envelope and type-specific validation;
- DDC entries are valid but are not referenced by the published manifest.

## 6. Standalone Validation Without DDC

```bash
mv Saved/Feature025Quickstart/DerivedDataCache \
  Saved/Feature025Quickstart/DerivedDataCache.hidden
mv Saved/Feature025Quickstart/Source \
  Saved/Feature025Quickstart/Source.hidden

Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker validate \
  --output Saved/Feature025Quickstart/Cooked \
  --strict-files \
  --normalized-report \
  --report Saved/Feature025Quickstart/published-validation.json

mv Saved/Feature025Quickstart/DerivedDataCache.hidden \
  Saved/Feature025Quickstart/DerivedDataCache
mv Saved/Feature025Quickstart/Source.hidden \
  Saved/Feature025Quickstart/Source
```

Expected: validation succeeds after both source and DDC removal, using only
`Current.json`, `Manifest.json`, the target evidence embedded by the manifest,
and referenced `.sgasset` files.

## 7. Unchanged Incremental Cook

```bash
Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker cook \
  --source-root Saved/Feature025Quickstart/Source \
  --cook-all \
  --target-profile Config/AssetCooker/Profiles/Mac-Vulkan.json \
  --output Saved/Feature025Quickstart/Cooked \
  --ddc Saved/Feature025Quickstart/DerivedDataCache \
  --workers 8 \
  --normalized-report \
  --report Saved/Feature025Quickstart/incremental.json
```

Expected:

- 100% of eligible payloads are cache hits;
- zero payloads are regenerated;
- generation ID and normalized output match the clean cook;
- changing worker count does not change any deterministic artifact.

## 8. Explicit Root Mode

Use an Asset ID printed by the plan report:

```bash
Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker cook \
  --source-root Saved/Feature025Quickstart/Source \
  --root 'StaticModel:representative.gltf#idx.scene.0' \
  --target-profile Config/AssetCooker/Profiles/Mac-Vulkan.json \
  --output Saved/Feature025Quickstart/RootCooked \
  --ddc Saved/Feature025Quickstart/DerivedDataCache \
  --normalized-report \
  --report Saved/Feature025Quickstart/root-cook.json
```

Expected: the manifest contains exactly the explicit root and its complete
required transitive dependency closure. Unrelated discovered assets are absent.

## 9. Strict Cache Validation

```bash
Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker validate-cache \
  --ddc Saved/Feature025Quickstart/DerivedDataCache \
  --normalized-report \
  --report Saved/Feature025Quickstart/cache-validation.json
```

Expected: exit code 0 for an unmodified DDC. The corruption test suite copies
this tree, mutates metadata/payload cases, and verifies strict failure while an
ordinary cook quarantines and rebuilds.

## 10. Inspect And Corruption Evidence

Use a derived key printed by `plan.json`; the key below is the representative
image fixture key for the canonical macOS Vulkan profile:

```bash
Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker inspect \
  --target-profile Config/AssetCooker/Profiles/Mac-Vulkan.json \
  --normalized-report \
  --report Saved/Feature025Quickstart/inspect-profile.json

Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker inspect \
  --output Saved/Feature025Quickstart/Cooked \
  --normalized-report \
  --report Saved/Feature025Quickstart/inspect-published.json

Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker inspect \
  --ddc Saved/Feature025Quickstart/DerivedDataCache \
  --key 9415b8853a2c33120afec9a66f167e20d76d772816014ef26f85034ac8a3ced1 \
  --normalized-report \
  --report Saved/Feature025Quickstart/inspect-cache.json

Build/Mac/Release/Tests/StonerTest \
  --suite asset-cooker-ddc \
  --suite asset-cooker-published-validation
```

Expected: all three inspect commands return canonical evidence and both
corruption suites reject their complete malformed case matrices without
damaging the original published generation or DDC.

## 11. Determinism And Scale Gates

```bash
Build/Mac/Release/Tests/StonerTest \
  --suite asset-cooker-determinism \
  --suite asset-cooker-benchmark \
  --asset-cooker-benchmark-profile reference \
  --asset-cooker-benchmark-report Validation/025/reports/performance-m4-pro.txt
```

The acceptance runner performs twenty plan/cook/incremental/validate repeats,
compares 1-worker and 8-worker normalized artifacts, and records Apple M4 Pro
reference telemetry separately from deterministic digests.

Reference budgets for 1,000 assets and 5,000 edges:

| Operation | Apple M4 Pro Release |
|---|---:|
| Plan | <=2 seconds |
| Fully cached incremental cook | <=10 seconds |
| Standalone generation validation | <=10 seconds |
| Synthetic graph clean cook | <=60 seconds |
| Representative Feature 021-024 clean cook | <=60 seconds |
| Synthetic corpus peak RSS | <1 GiB |

The local reference runner hard-fails every listed M4 threshold. Hosted CI uses
separate hard 4x time smoke ceilings and does not replace the local reference
benchmark.

## 12. Architecture And Repository Gates

```bash
conda run -n godot python Tests/verify_asset_layer.py
conda run -n godot python Tests/verify_architecture.py
conda run -n godot python Tests/verify_asset_cooker_contracts.py
git status --short
```

Expected:

- no runtime module includes or links `Tools/AssetCooker`;
- Asset has no RHI, Renderer, Application, Backend, or graphics API dependency;
- Tools/AssetCooker links only Asset and Core;
- native lock/replace types remain Core-private;
- DDC and cooked output roots are ignored and not staged;
- only source profiles, fixtures, schemas, and normalized `Validation/025`
  evidence are tracked.

## 13. Full Regression And CI

```bash
Build/Mac/Debug/Tests/StonerTest
Build/Mac/Release/Tests/StonerTest
```

Required hosted jobs:

- Windows, macOS, Linux Debug;
- Windows, macOS, Linux strict Release;
- Linux ASan + UBSan;
- Linux TSan for AssetCooker scheduler/DDC paths;
- two-process publication lease and atomicity probe;
- clean-machine cook, incremental convergence, strict validation, and retained
  normalized report artifacts.
