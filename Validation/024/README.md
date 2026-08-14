# Feature 024 Validation

Status: **complete**

This directory holds reproducible evidence for the Static Mesh and Model
Pipeline. Reports are normalized text or JSON, use repository-relative paths,
and must name their command, profile, source revision, fixture-manifest digest,
and result. Large generated outputs remain CI artifacts rather than checked-in
logs.

Run the focused static-model verification through the `asset-static-model`,
`asset-gltf-malformed`, and `renderer-static-mesh` suites. The checked-in
`reports/quickstart.txt` records the local closeout candidate; CI-native
readback and cross-platform artifact hashes remain separate completion gates.

Run fixture and architecture checks directly with:

```text
python Tests/verify_static_model_fixtures.py \
  --manifest Validation/024/fixture-manifest.json \
  --fixtures Tests/Fixtures/StaticModel
python Tests/verify_asset_layer.py --root .
```

Feature 018 and 019 deterministic migration reports prove that the Unreal-style
coordinate convention did not regress their CPU-visible behavior. They are not
substitutes for native Vulkan evidence.

## Closeout

Feature 024 completed on commit
`945076d5074c1256bec6ac6c841fc19449fc5e85`. GitHub Actions run
[31766671726](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/31766671726)
passed Windows/macOS/Linux Debug and strict Release, full regressions, Linux
ASan/UBSan, TSan, Feature 018 endurance, and Feature 019 native readback. Native
probe run
[31766671729](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/31766671729)
passed real Lavapipe indexed-clockwise static-mesh attachment readback and
Renderer buffer transfer.

The Apple M4 Pro reference gate passed 20-run fixture determinism and five
measured representative imports; the slowest measured import was 0.0152 s and
tracked request-owned peak memory was 13,196,337 bytes. See
`reports/quickstart.txt`, `reports/performance.json`, and
`reports/cross-platform-ci.md` for exact commands, tolerances, artifact IDs,
and hashes. No accepted Feature 024 gap remains.
