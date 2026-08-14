# Feature 024 Validation

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
