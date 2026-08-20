# Feature 027 US1 Shared Regression Checkpoint

Status: PASS on the local macOS arm64 working-tree checkpoint. This is not
native Metal evidence because the current execution sandbox exposes no Metal
device.

## Commands

- `conda run -n godot scons -j8 config=debug strict=1`: PASS.
- `conda run -n godot python -m unittest Tests/test_verify_metal_backend.py Tests/test_verify_architecture.py`: PASS, 16 tests.
- Focused `rhi`, `vulkan`, `vulkan-native`, `renderer-forward`,
  `renderer-render-graph`, `renderer-material-asset`, `deferred-renderer`,
  `deferred-native`, and `triangle-demo` suites: PASS.
- Focused `metal`, `metal-presentation`, and opt-out
  `metal-presentation-visible` suites: PASS with native execution classified
  as controlled unavailable.

The MoltenVK loader also reported Metal unavailable in this sandbox. No native
or visible acceptance claim is derived from these results.
