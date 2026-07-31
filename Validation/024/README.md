# Feature 024 Validation

This directory holds reproducible evidence for the Static Mesh and Model
Pipeline. Reports are normalized text or JSON, use repository-relative paths,
and must name their command, profile, source revision, fixture-manifest digest,
and result. Large generated outputs remain CI artifacts rather than checked-in
logs.

Run the focused static-model verification through the `asset-static-model` and
`renderer-static-mesh` suites once they are introduced. Run Python provenance
checks directly with `python Tests/verify_static_model_provenance.py`.
